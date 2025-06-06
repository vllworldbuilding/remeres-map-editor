//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////
// Remere's Map Editor is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Remere's Map Editor is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
//////////////////////////////////////////////////////////////////////

#include "main.h"

#if defined(__LINUX__) || defined(__WINDOWS__)
	#include <GL/glut.h>
#endif

#include "editor.h"
#include "gui.h"
#include "sprites.h"
#include "map_drawer.h"
#include "map_display.h"
#include "copybuffer.h"
#include "live_socket.h"
#include "graphics.h"

#include "doodad_brush.h"
#include "monster_brush.h"
#include "house_exit_brush.h"
#include "house_brush.h"
#include "spawn_monster_brush.h"
#include "sprite_appearances.h"
#include "npc_brush.h"
#include "spawn_npc_brush.h"
#include "wall_brush.h"
#include "carpet_brush.h"
#include "raw_brush.h"
#include "table_brush.h"
#include "waypoint_brush.h"
#include "zone_brush.h"
#include "light_drawer.h"

DrawingOptions::DrawingOptions() {
	SetDefault();
}

void DrawingOptions::SetDefault() {
	transparent_floors = false;
	transparent_items = false;
	show_ingame_box = false;
	show_lights = false;
	show_light_strength = true;
	ingame = false;
	dragging = false;

	show_grid = 0;
	show_all_floors = true;
	show_monsters = true;
	show_spawns_monster = true;
	show_npcs = true;
	show_spawns_npc = true;
	show_houses = true;
	show_shade = true;
	show_special_tiles = true;
	show_items = true;

	highlight_items = false;
	show_blocking = false;
	show_tooltips = false;
	show_as_minimap = false;
	show_only_colors = false;
	show_only_modified = false;
	show_preview = false;
	show_hooks = false;
	show_pickupables = false;
	show_moveables = false;
	show_avoidables = false;
	hide_items_when_zoomed = true;
}

void DrawingOptions::SetIngame() {
	transparent_floors = false;
	transparent_items = false;
	show_ingame_box = false;
	show_lights = false;
	show_light_strength = false;
	ingame = true;
	dragging = false;

	show_grid = 0;
	show_all_floors = true;
	show_monsters = true;
	show_spawns_monster = false;
	show_npcs = true;
	show_spawns_npc = false;
	show_houses = false;
	show_shade = false;
	show_special_tiles = false;
	show_items = true;

	highlight_items = false;
	show_blocking = false;
	show_tooltips = false;
	show_as_minimap = false;
	show_only_colors = false;
	show_only_modified = false;
	show_preview = false;
	show_hooks = false;
	show_pickupables = false;
	show_moveables = false;
	show_avoidables = false;
	hide_items_when_zoomed = false;
}

bool DrawingOptions::isOnlyColors() const noexcept {
	return show_as_minimap || show_only_colors;
}

bool DrawingOptions::isTileIndicators() const noexcept {
	if (isOnlyColors()) {
		return false;
	}
	return show_pickupables || show_moveables || show_houses || show_spawns_monster || show_spawns_npc;
}

bool DrawingOptions::isTooltips() const noexcept {
	return show_tooltips && !isOnlyColors();
}

MapDrawer::MapDrawer(MapCanvas* canvas) :
	canvas(canvas), editor(canvas->editor) {
	light_drawer = std::make_shared<LightDrawer>();
}

MapDrawer::~MapDrawer() {
	Release();
}

void MapDrawer::SetupVars() {
	canvas->MouseToMap(&mouse_map_x, &mouse_map_y);
	canvas->GetViewBox(&view_scroll_x, &view_scroll_y, &screensize_x, &screensize_y);

	dragging = canvas->dragging;
	dragging_draw = canvas->dragging_draw;

	zoom = static_cast<float>(canvas->GetZoom());
	tile_size = int(rme::TileSize / zoom); // after zoom
	floor = canvas->GetFloor();

	if (options.show_all_floors) {
		if (floor < 8) {
			start_z = rme::MapGroundLayer;
		} else {
			start_z = std::min(rme::MapMaxLayer, floor + 2);
		}
	} else {
		start_z = floor;
	}

	end_z = floor;
	superend_z = (floor > rme::MapGroundLayer ? 8 : 0);

	start_x = view_scroll_x / rme::TileSize;
	start_y = view_scroll_y / rme::TileSize;

	if (floor > rme::MapGroundLayer) {
		start_x -= 2;
		start_y -= 2;
	}

	end_x = start_x + screensize_x / tile_size + 2;
	end_y = start_y + screensize_y / tile_size + 2;
}

void MapDrawer::SetupGL() {
	glViewport(0, 0, screensize_x, screensize_y);

	// Enable 2D mode
	int vPort[4];

	glGetIntegerv(GL_VIEWPORT, vPort);

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glOrtho(0, vPort[2] * zoom, vPort[3] * zoom, 0, -1, 1);

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	glTranslatef(0.375f, 0.375f, 0.0f);
}

void MapDrawer::Release() {
	for (auto it = tooltips.begin(); it != tooltips.end(); ++it) {
		delete *it;
	}
	tooltips.clear();

	if (light_drawer) {
		light_drawer->clear();
	}

	// Disable 2D mode
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
}

void MapDrawer::Draw() {
	DrawBackground();
	DrawMap();
	if (options.show_lights) {
		light_drawer->draw(start_x, start_y, end_x, end_y, view_scroll_x, view_scroll_y);
	}
	DrawDraggingShadow();
	DrawHigherFloors();
	if (options.dragging) {
		DrawSelectionBox();
	}
	DrawLiveCursors();
	DrawBrush();
	if (options.show_grid && zoom <= 10.f) {
		DrawGrid();
	}
	if (options.show_ingame_box) {
		DrawIngameBox();
	}
	if (options.isTooltips()) {
		DrawTooltips();
	}
}

void MapDrawer::DrawBackground() {
	// Black Background
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glLoadIdentity();

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);

	// glAlphaFunc(GL_GEQUAL, 0.9f);
	// glEnable(GL_ALPHA_TEST);
}

inline int getFloorAdjustment(int floor) {
	if (floor > rme::MapGroundLayer) { // Underground
		return 0; // No adjustment
	} else {
		return rme::TileSize * (rme::MapGroundLayer - floor);
	}
}

void MapDrawer::DrawShade(int map_z) {
	if (map_z == end_z && start_z != end_z) {
		bool only_colors = options.isOnlyColors();
		if (!only_colors) {
			glDisable(GL_TEXTURE_2D);
		}

		float x = screensize_x * zoom;
		float y = screensize_y * zoom;
		glColor4ub(0, 0, 0, 128);
		glBegin(GL_QUADS);
		glVertex2f(0, y);
		glVertex2f(x, y);
		glVertex2f(x, 0);
		glVertex2f(0, 0);
		glEnd();

		if (!only_colors) {
			glEnable(GL_TEXTURE_2D);
		}
	}
}

void MapDrawer::DrawMap() {
	bool live_client = editor.IsLiveClient();

	Brush* brush = g_gui.GetCurrentBrush();

	// The current house we're drawing
	current_house_id = 0;
	if (brush) {
		if (brush->isHouse()) {
			current_house_id = brush->asHouse()->getHouseID();
		} else if (brush->isHouseExit()) {
			current_house_id = brush->asHouseExit()->getHouseID();
		}
	}

	bool only_colors = options.isOnlyColors();
	bool tile_indicators = options.isTileIndicators();

	for (int map_z = start_z; map_z >= superend_z; map_z--) {
		if (options.show_shade) {
			DrawShade(map_z);
		}

		if (map_z >= end_z) {
			if (!only_colors) {
				glEnable(GL_TEXTURE_2D);
			}

			int nd_start_x = start_x & ~3;
			int nd_start_y = start_y & ~3;
			int nd_end_x = (end_x & ~3) + 4;
			int nd_end_y = (end_y & ~3) + 4;

			for (int nd_map_x = nd_start_x; nd_map_x <= nd_end_x; nd_map_x += 4) {
				for (int nd_map_y = nd_start_y; nd_map_y <= nd_end_y; nd_map_y += 4) {
					QTreeNode* nd = editor.getMap().getLeaf(nd_map_x, nd_map_y);
					if (!nd) {
						if (!live_client) {
							continue;
						}
						nd = editor.getMap().createLeaf(nd_map_x, nd_map_y);
						nd->setVisible(false, false);
					}

					if (!live_client || nd->isVisible(map_z > rme::MapGroundLayer)) {
						for (int map_x = 0; map_x < 4; ++map_x) {
							for (int map_y = 0; map_y < 4; ++map_y) {
								TileLocation* location = nd->getTile(map_x, map_y, map_z);
								DrawTile(location);
								// draw light, but only if not zoomed too far
								if (location && options.show_lights && zoom <= 10) {
									AddLight(location);
								}
							}
						}
						if (tile_indicators) {
							for (int map_x = 0; map_x < 4; ++map_x) {
								for (int map_y = 0; map_y < 4; ++map_y) {
									DrawTileIndicators(nd->getTile(map_x, map_y, map_z));
								}
							}
						}
					} else {
						if (!nd->isRequested(map_z > rme::MapGroundLayer)) {
							// Request the node
							editor.QueryNode(nd_map_x, nd_map_y, map_z > rme::MapGroundLayer);
							nd->setRequested(map_z > rme::MapGroundLayer, true);
						}
						int cy = (nd_map_y)*rme::TileSize - view_scroll_y - getFloorAdjustment(floor);
						int cx = (nd_map_x)*rme::TileSize - view_scroll_x - getFloorAdjustment(floor);

						glColor4ub(255, 0, 255, 128);
						glBegin(GL_QUADS);
						glVertex2f(cx, cy + rme::TileSize * 4);
						glVertex2f(cx + rme::TileSize * 4, cy + rme::TileSize * 4);
						glVertex2f(cx + rme::TileSize * 4, cy);
						glVertex2f(cx, cy);
						glEnd();
					}
				}
			}

			if (!only_colors) {
				glDisable(GL_TEXTURE_2D);
			}

			DrawPositionIndicator(map_z);
		}

		// Draws the doodad preview or the paste preview (or import preview)
		DrawSecondaryMap(map_z);

		--start_x;
		--start_y;
		++end_x;
		++end_y;
	}

	if (!only_colors) {
		glEnable(GL_TEXTURE_2D);
	}
}

void MapDrawer::DrawSecondaryMap(int map_z) {
	if (options.ingame) {
		return;
	}

	BaseMap* secondary_map = g_gui.secondary_map;
	if (!secondary_map) {
		return;
	}

	Position normal_pos;
	Position to_pos(mouse_map_x, mouse_map_y, floor);

	if (canvas->isPasting()) {
		normal_pos = editor.copybuffer.getPosition();
	} else {
		Brush* brush = g_gui.GetCurrentBrush();
		if (brush && brush->isDoodad()) {
			normal_pos = Position(0x8000, 0x8000, 0x8);
		}
	}

	glEnable(GL_TEXTURE_2D);

	for (int map_x = start_x; map_x <= end_x; map_x++) {
		for (int map_y = start_y; map_y <= end_y; map_y++) {
			Position final_pos(map_x, map_y, map_z);
			Position pos = normal_pos + final_pos - to_pos;
			if (pos.z < 0 || pos.z >= rme::MapLayers) {
				continue;
			}

			Tile* tile = secondary_map->getTile(pos);
			if (!tile) {
				continue;
			}

			int draw_x, draw_y;
			getDrawPosition(final_pos, draw_x, draw_y);

			// Draw ground
			uint8_t r = 160, g = 160, b = 160;
			if (tile->ground) {
				if (options.show_blocking && tile->isBlocking()) {
					g = g / 3 * 2;
					b = b / 3 * 2;
				}
				if (options.show_houses && tile->isHouseTile()) {
					if (tile->getHouseID() == current_house_id) {
						r /= 2;
					} else {
						r /= 2;
						g /= 2;
					}
				} else if (options.show_special_tiles && tile->isPZ()) {
					r /= 2;
					b /= 2;
				}
				if (options.show_special_tiles && tile->getMapFlags() & TILESTATE_PVPZONE) {
					r = r / 3 * 2;
					b = r / 3 * 2;
				}
				if (options.show_special_tiles && tile->hasZone(g_gui.zone_brush->getZone())) {
					r = r / 3 * 2;
					b = b / 3 * 2;
				}
				if (options.show_special_tiles && tile->getMapFlags() & TILESTATE_NOLOGOUT) {
					b /= 2;
				}
				if (options.show_special_tiles && tile->getMapFlags() & TILESTATE_NOPVP) {
					g /= 2;
				}
				BlitItem(draw_x, draw_y, tile, tile->ground, true, r, g, b, 160);
			}

			bool hidden = options.hide_items_when_zoomed && zoom > 10.f;

			// Draw items
			if (!hidden && !tile->items.empty()) {
				for (const Item* item : tile->items) {
					if (item->isBorder()) {
						BlitItem(draw_x, draw_y, tile, item, true, 160, r, g, b);
					} else {
						BlitItem(draw_x, draw_y, tile, item, true, 160, 160, 160, 160);
					}
				}
			}

			// Monsters
			if (!hidden && options.show_monsters && !tile->monsters.empty()) {
				for (auto monster : tile->monsters) {
					BlitCreature(draw_x, draw_y, monster);
				}
			}

			// NPCS
			if (!hidden && options.show_npcs && tile->npc) {
				BlitCreature(draw_x, draw_y, tile->npc);
			}
		}
	}

	glDisable(GL_TEXTURE_2D);
}

void MapDrawer::DrawIngameBox() {
	int center_x = start_x + int(screensize_x * zoom / 64);
	int center_y = start_y + int(screensize_y * zoom / 64);

	int offset_y = 2;
	int box_start_map_x = center_x;
	int box_start_map_y = center_y + offset_y;
	int box_end_map_x = center_x + rme::ClientMapWidth;
	int box_end_map_y = center_y + rme::ClientMapHeight + offset_y;

	int box_start_x = box_start_map_x * rme::TileSize - view_scroll_x;
	int box_start_y = box_start_map_y * rme::TileSize - view_scroll_y;
	int box_end_x = box_end_map_x * rme::TileSize - view_scroll_x;
	int box_end_y = box_end_map_y * rme::TileSize - view_scroll_y;

	static wxColor side_color(0, 0, 0, 200);

	glDisable(GL_TEXTURE_2D);

	// left side
	if (box_start_map_x >= start_x) {
		drawFilledRect(0, 0, box_start_x, screensize_y * zoom, side_color);
	}

	// right side
	if (box_end_map_x < end_x) {
		drawFilledRect(box_end_x, 0, screensize_x * zoom, screensize_y * zoom, side_color);
	}

	// top side
	if (box_start_map_y >= start_y) {
		drawFilledRect(box_start_x, 0, box_end_x - box_start_x, box_start_y, side_color);
	}

	// bottom side
	if (box_end_map_y < end_y) {
		drawFilledRect(box_start_x, box_end_y, box_end_x - box_start_x, screensize_y * zoom, side_color);
	}

	// hidden tiles
	drawRect(box_start_x, box_start_y, box_end_x - box_start_x, box_end_y - box_start_y, *wxRED);

	// visible tiles
	box_start_x += rme::TileSize;
	box_start_y += rme::TileSize;
	box_end_x -= 2 * rme::TileSize;
	box_end_y -= 2 * rme::TileSize;
	drawRect(box_start_x, box_start_y, box_end_x - box_start_x, box_end_y - box_start_y, *wxGREEN);

	// player position
	box_start_x += ((rme::ClientMapWidth / 2) - 2) * rme::TileSize;
	box_start_y += ((rme::ClientMapHeight / 2) - 2) * rme::TileSize;
	box_end_x = box_start_x + rme::TileSize;
	box_end_y = box_start_y + rme::TileSize;
	drawRect(box_start_x, box_start_y, box_end_x - box_start_x, box_end_y - box_start_y, *wxGREEN);

	glEnable(GL_TEXTURE_2D);
}

void MapDrawer::DrawGrid() {
	glDisable(GL_TEXTURE_2D);
	glColor4ub(255, 255, 255, 128);
	glBegin(GL_LINES);

	for (int y = start_y; y < end_y; ++y) {
		int py = y * rme::TileSize - view_scroll_y;
		glVertex2f(start_x * rme::TileSize - view_scroll_x, py);
		glVertex2f(end_x * rme::TileSize - view_scroll_x, py);
	}

	for (int x = start_x; x < end_x; ++x) {
		int px = x * rme::TileSize - view_scroll_x;
		glVertex2f(px, start_y * rme::TileSize - view_scroll_y);
		glVertex2f(px, end_y * rme::TileSize - view_scroll_y);
	}

	glEnd();
	glEnable(GL_TEXTURE_2D);
}

void MapDrawer::DrawDraggingShadow() {
	if (!dragging || options.ingame || editor.getSelection().isBusy()) {
		return;
	}

	glEnable(GL_TEXTURE_2D);

	for (Tile* tile : editor.getSelection()) {
		int move_z = canvas->drag_start_z - floor;
		int move_x = canvas->drag_start_x - mouse_map_x;
		int move_y = canvas->drag_start_y - mouse_map_y;

		if (move_x == 0 && move_y == 0 && move_z == 0) {
			continue;
		}

		const Position &position = tile->getPosition();
		int pos_z = position.z - move_z;
		if (pos_z < 0 || pos_z >= rme::MapLayers) {
			continue;
		}

		int pos_x = position.x - move_x;
		int pos_y = position.y - move_y;

		// On screen and dragging?
		if (pos_x + 2 > start_x && pos_x < end_x && pos_y + 2 > start_y && pos_y < end_y) {
			Position pos(pos_x, pos_y, pos_z);
			int draw_x, draw_y;
			getDrawPosition(pos, draw_x, draw_y);

			ItemVector items = tile->getSelectedItems();
			Tile* dest_tile = editor.getMap().getTile(pos);

			for (Item* item : items) {
				if (dest_tile) {
					BlitItem(draw_x, draw_y, dest_tile, item, true, 160, 160, 160, 160);
				} else {
					BlitItem(draw_x, draw_y, pos, item, true, 160, 160, 160, 160);
				}
			}

			if (options.show_monsters && !tile->monsters.empty()) {
				for (auto monster : tile->monsters) {
					if (!monster->isSelected()) {
						continue;
					}

					BlitCreature(draw_x, draw_y, monster);
				}
			}

			if (tile->spawnMonster && tile->spawnMonster->isSelected()) {
				DrawIndicator(draw_x, draw_y, EDITOR_SPRITE_MONSTERS, 160, 160, 160, 160);
			}

			if (options.show_npcs && tile->npc && tile->npc->isSelected()) {
				BlitCreature(draw_x, draw_y, tile->npc);
			}
			if (tile->spawnNpc && tile->spawnNpc->isSelected()) {
				DrawIndicator(draw_x, draw_y, EDITOR_SPRITE_NPCS, 160, 160, 160, 160);
			}
		}
	}

	glDisable(GL_TEXTURE_2D);
}

void MapDrawer::DrawHigherFloors() {
	if (!options.transparent_floors || floor == 0 || floor == 8) {
		return;
	}

	glEnable(GL_TEXTURE_2D);

	int map_z = floor - 1;
	for (int map_x = start_x; map_x <= end_x; map_x++) {
		for (int map_y = start_y; map_y <= end_y; map_y++) {
			Tile* tile = editor.getMap().getTile(map_x, map_y, map_z);
			if (!tile) {
				continue;
			}

			int draw_x, draw_y;
			getDrawPosition(tile->getPosition(), draw_x, draw_y);

			if (tile->ground) {
				if (tile->isPZ()) {
					BlitItem(draw_x, draw_y, tile, tile->ground, false, 128, 255, 128, 96);
				} else {
					BlitItem(draw_x, draw_y, tile, tile->ground, false, 255, 255, 255, 96);
				}
			}

			bool hidden = options.hide_items_when_zoomed && zoom > 10.f;
			if (!hidden && !tile->items.empty()) {
				for (const Item* item : tile->items) {
					BlitItem(draw_x, draw_y, tile, item, false, 255, 255, 255, 96);
				}
			}
		}
	}

	glDisable(GL_TEXTURE_2D);
}

void MapDrawer::DrawSelectionBox() {
	if (options.ingame) {
		return;
	}

	// Draw bounding box

	int last_click_rx = canvas->last_click_abs_x - view_scroll_x;
	int last_click_ry = canvas->last_click_abs_y - view_scroll_y;
	double cursor_rx = canvas->cursor_x * zoom;
	double cursor_ry = canvas->cursor_y * zoom;

	static double lines[4][4];

	lines[0][0] = last_click_rx;
	lines[0][1] = last_click_ry;
	lines[0][2] = cursor_rx;
	lines[0][3] = last_click_ry;

	lines[1][0] = cursor_rx;
	lines[1][1] = last_click_ry;
	lines[1][2] = cursor_rx;
	lines[1][3] = cursor_ry;

	lines[2][0] = cursor_rx;
	lines[2][1] = cursor_ry;
	lines[2][2] = last_click_rx;
	lines[2][3] = cursor_ry;

	lines[3][0] = last_click_rx;
	lines[3][1] = cursor_ry;
	lines[3][2] = last_click_rx;
	lines[3][3] = last_click_ry;

	glDisable(GL_TEXTURE_2D);
	glEnable(GL_LINE_STIPPLE);
	glLineStipple(2, 0xAAAA);
	glLineWidth(1.0);
	glColor4f(1.0, 1.0, 1.0, 1.0);
	glBegin(GL_LINES);
	for (int i = 0; i < 4; i++) {
		glVertex2f(lines[i][0], lines[i][1]);
		glVertex2f(lines[i][2], lines[i][3]);
	}
	glEnd();
	glDisable(GL_LINE_STIPPLE);
	glEnable(GL_TEXTURE_2D);
}

void MapDrawer::DrawLiveCursors() {
	if (options.ingame || !editor.IsLive()) {
		return;
	}

	LiveSocket &live = editor.GetLive();
	for (LiveCursor &cursor : live.getCursorList()) {
		if (cursor.pos.z <= rme::MapGroundLayer && floor > rme::MapGroundLayer) {
			continue;
		}

		if (cursor.pos.z > rme::MapGroundLayer && floor <= 8) {
			continue;
		}

		if (cursor.pos.z < floor) {
			cursor.color = wxColor(
				cursor.color.Red(),
				cursor.color.Green(),
				cursor.color.Blue(),
				std::max<uint8_t>(cursor.color.Alpha() / 2, 64)
			);
		}

		int offset;
		if (cursor.pos.z <= rme::MapGroundLayer) {
			offset = (rme::MapGroundLayer - cursor.pos.z) * rme::TileSize;
		} else {
			offset = rme::TileSize * (floor - cursor.pos.z);
		}

		float draw_x = ((cursor.pos.x * rme::TileSize) - view_scroll_x) - offset;
		float draw_y = ((cursor.pos.y * rme::TileSize) - view_scroll_y) - offset;

		glColor(cursor.color);
		glBegin(GL_QUADS);
		glVertex2f(draw_x, draw_y);
		glVertex2f(draw_x + rme::TileSize, draw_y);
		glVertex2f(draw_x + rme::TileSize, draw_y + rme::TileSize);
		glVertex2f(draw_x, draw_y + rme::TileSize);
		glEnd();
	}
}

void MapDrawer::DrawBrush() {
	if (options.ingame || !g_gui.IsDrawingMode() || !g_gui.GetCurrentBrush()) {
		return;
	}

	Brush* brush = g_gui.GetCurrentBrush();

	BrushColor brushColor = COLOR_BLANK;
	if (brush->isTerrain() || brush->isTable() || brush->isCarpet()) {
		brushColor = COLOR_BRUSH;
	} else if (brush->isHouse()) {
		brushColor = COLOR_HOUSE_BRUSH;
	} else if (brush->isFlag()) {
		brushColor = COLOR_FLAG_BRUSH;
	} else if (brush->isSpawnMonster()) {
		brushColor = COLOR_SPAWN_BRUSH;
	} else if (brush->isSpawnNpc()) {
		brushColor = COLOR_SPAWN_NPC_BRUSH;
	} else if (brush->isEraser()) {
		brushColor = COLOR_ERASER;
	}

	int adjustment = getFloorAdjustment(floor);

	if (dragging_draw) {
		ASSERT(brush->canDrag());

		if (brush->isWall()) {
			int last_click_start_map_x = std::min(canvas->last_click_map_x, mouse_map_x);
			int last_click_start_map_y = std::min(canvas->last_click_map_y, mouse_map_y);
			int last_click_end_map_x = std::max(canvas->last_click_map_x, mouse_map_x) + 1;
			int last_click_end_map_y = std::max(canvas->last_click_map_y, mouse_map_y) + 1;

			int last_click_start_sx = last_click_start_map_x * rme::TileSize - view_scroll_x - adjustment;
			int last_click_start_sy = last_click_start_map_y * rme::TileSize - view_scroll_y - adjustment;
			int last_click_end_sx = last_click_end_map_x * rme::TileSize - view_scroll_x - adjustment;
			int last_click_end_sy = last_click_end_map_y * rme::TileSize - view_scroll_y - adjustment;

			int delta_x = last_click_end_sx - last_click_start_sx;
			int delta_y = last_click_end_sy - last_click_start_sy;

			glColor(brushColor);
			glBegin(GL_QUADS);
			{
				glVertex2f(last_click_start_sx, last_click_start_sy + rme::TileSize);
				glVertex2f(last_click_end_sx, last_click_start_sy + rme::TileSize);
				glVertex2f(last_click_end_sx, last_click_start_sy);
				glVertex2f(last_click_start_sx, last_click_start_sy);
			}

			if (delta_y > rme::TileSize) {
				glVertex2f(last_click_start_sx, last_click_end_sy - rme::TileSize);
				glVertex2f(last_click_start_sx + rme::TileSize, last_click_end_sy - rme::TileSize);
				glVertex2f(last_click_start_sx + rme::TileSize, last_click_start_sy + rme::TileSize);
				glVertex2f(last_click_start_sx, last_click_start_sy + rme::TileSize);
			}

			if (delta_x > rme::TileSize && delta_y > rme::TileSize) {
				glVertex2f(last_click_end_sx - rme::TileSize, last_click_start_sy + rme::TileSize);
				glVertex2f(last_click_end_sx, last_click_start_sy + rme::TileSize);
				glVertex2f(last_click_end_sx, last_click_end_sy - rme::TileSize);
				glVertex2f(last_click_end_sx - rme::TileSize, last_click_end_sy - rme::TileSize);
			}

			if (delta_y > rme::TileSize) {
				glVertex2f(last_click_start_sx, last_click_end_sy - rme::TileSize);
				glVertex2f(last_click_end_sx, last_click_end_sy - rme::TileSize);
				glVertex2f(last_click_end_sx, last_click_end_sy);
				glVertex2f(last_click_start_sx, last_click_end_sy);
			}
			glEnd();
		} else {
			if (brush->isRaw()) {
				glEnable(GL_TEXTURE_2D);
			}

			if (g_gui.GetBrushShape() == BRUSHSHAPE_SQUARE || brush->isSpawnMonster() || brush->isSpawnNpc()) {
				if (brush->isRaw() || brush->isOptionalBorder()) {
					int start_x, end_x;
					int start_y, end_y;

					if (mouse_map_x < canvas->last_click_map_x) {
						start_x = mouse_map_x;
						end_x = canvas->last_click_map_x;
					} else {
						start_x = canvas->last_click_map_x;
						end_x = mouse_map_x;
					}
					if (mouse_map_y < canvas->last_click_map_y) {
						start_y = mouse_map_y;
						end_y = canvas->last_click_map_y;
					} else {
						start_y = canvas->last_click_map_y;
						end_y = mouse_map_y;
					}

					RAWBrush* raw_brush = nullptr;
					if (brush->isRaw()) {
						raw_brush = brush->asRaw();
					}

					for (int y = start_y; y <= end_y; y++) {
						int cy = y * rme::TileSize - view_scroll_y - adjustment;
						for (int x = start_x; x <= end_x; x++) {
							int cx = x * rme::TileSize - view_scroll_x - adjustment;
							if (brush->isOptionalBorder()) {
								glColorCheck(brush, Position(x, y, floor));
							} else {
								BlitSpriteType(cx, cy, raw_brush->getItemType()->sprite, 160, 160, 160, 160);
							}
						}
					}
				} else {
					int last_click_start_map_x = std::min(canvas->last_click_map_x, mouse_map_x);
					int last_click_start_map_y = std::min(canvas->last_click_map_y, mouse_map_y);
					int last_click_end_map_x = std::max(canvas->last_click_map_x, mouse_map_x) + 1;
					int last_click_end_map_y = std::max(canvas->last_click_map_y, mouse_map_y) + 1;

					int last_click_start_sx = last_click_start_map_x * rme::TileSize - view_scroll_x - adjustment;
					int last_click_start_sy = last_click_start_map_y * rme::TileSize - view_scroll_y - adjustment;
					int last_click_end_sx = last_click_end_map_x * rme::TileSize - view_scroll_x - adjustment;
					int last_click_end_sy = last_click_end_map_y * rme::TileSize - view_scroll_y - adjustment;

					glColor(brushColor);
					glBegin(GL_QUADS);
					glVertex2f(last_click_start_sx, last_click_start_sy);
					glVertex2f(last_click_end_sx, last_click_start_sy);
					glVertex2f(last_click_end_sx, last_click_end_sy);
					glVertex2f(last_click_start_sx, last_click_end_sy);
					glEnd();
				}
			} else if (g_gui.GetBrushShape() == BRUSHSHAPE_CIRCLE) {
				// Calculate drawing offsets
				int start_x, end_x;
				int start_y, end_y;
				int width = std::max(
					std::abs(std::max(mouse_map_y, canvas->last_click_map_y) - std::min(mouse_map_y, canvas->last_click_map_y)),
					std::abs(std::max(mouse_map_x, canvas->last_click_map_x) - std::min(mouse_map_x, canvas->last_click_map_x))
				);

				if (mouse_map_x < canvas->last_click_map_x) {
					start_x = canvas->last_click_map_x - width;
					end_x = canvas->last_click_map_x;
				} else {
					start_x = canvas->last_click_map_x;
					end_x = canvas->last_click_map_x + width;
				}

				if (mouse_map_y < canvas->last_click_map_y) {
					start_y = canvas->last_click_map_y - width;
					end_y = canvas->last_click_map_y;
				} else {
					start_y = canvas->last_click_map_y;
					end_y = canvas->last_click_map_y + width;
				}

				int center_x = start_x + (end_x - start_x) / 2;
				int center_y = start_y + (end_y - start_y) / 2;
				float radii = width / 2.0f + 0.005f;

				RAWBrush* raw_brush = nullptr;
				if (brush->isRaw()) {
					raw_brush = brush->asRaw();
				}

				for (int y = start_y - 1; y <= end_y + 1; y++) {
					int cy = y * rme::TileSize - view_scroll_y - adjustment;
					float dy = center_y - y;
					for (int x = start_x - 1; x <= end_x + 1; x++) {
						int cx = x * rme::TileSize - view_scroll_x - adjustment;

						float dx = center_x - x;
						// printf("%f;%f\n", dx, dy);
						float distance = sqrt(dx * dx + dy * dy);
						if (distance < radii) {
							if (brush->isRaw()) {
								BlitSpriteType(cx, cy, raw_brush->getItemType()->sprite, 160, 160, 160, 160);
							} else {
								glColor(brushColor);
								glBegin(GL_QUADS);
								glVertex2f(cx, cy + rme::TileSize);
								glVertex2f(cx + rme::TileSize, cy + rme::TileSize);
								glVertex2f(cx + rme::TileSize, cy);
								glVertex2f(cx, cy);
								glEnd();
							}
						}
					}
				}
			}

			if (brush->isRaw()) {
				glDisable(GL_TEXTURE_2D);
			}
		}
	} else {
		if (brush->isWall()) {
			int start_map_x = mouse_map_x - g_gui.GetBrushSize();
			int start_map_y = mouse_map_y - g_gui.GetBrushSize();
			int end_map_x = mouse_map_x + g_gui.GetBrushSize() + 1;
			int end_map_y = mouse_map_y + g_gui.GetBrushSize() + 1;

			int start_sx = start_map_x * rme::TileSize - view_scroll_x - adjustment;
			int start_sy = start_map_y * rme::TileSize - view_scroll_y - adjustment;
			int end_sx = end_map_x * rme::TileSize - view_scroll_x - adjustment;
			int end_sy = end_map_y * rme::TileSize - view_scroll_y - adjustment;

			int delta_x = end_sx - start_sx;
			int delta_y = end_sy - start_sy;

			glColor(brushColor);
			glBegin(GL_QUADS);
			{
				glVertex2f(start_sx, start_sy + rme::TileSize);
				glVertex2f(end_sx, start_sy + rme::TileSize);
				glVertex2f(end_sx, start_sy);
				glVertex2f(start_sx, start_sy);
			}

			if (delta_y > rme::TileSize) {
				glVertex2f(start_sx, end_sy - rme::TileSize);
				glVertex2f(start_sx + rme::TileSize, end_sy - rme::TileSize);
				glVertex2f(start_sx + rme::TileSize, start_sy + rme::TileSize);
				glVertex2f(start_sx, start_sy + rme::TileSize);
			}

			if (delta_x > rme::TileSize && delta_y > rme::TileSize) {
				glVertex2f(end_sx - rme::TileSize, start_sy + rme::TileSize);
				glVertex2f(end_sx, start_sy + rme::TileSize);
				glVertex2f(end_sx, end_sy - rme::TileSize);
				glVertex2f(end_sx - rme::TileSize, end_sy - rme::TileSize);
			}

			if (delta_y > rme::TileSize) {
				glVertex2f(start_sx, end_sy - rme::TileSize);
				glVertex2f(end_sx, end_sy - rme::TileSize);
				glVertex2f(end_sx, end_sy);
				glVertex2f(start_sx, end_sy);
			}
			glEnd();
		} else if (brush->isDoor()) {
			int cx = (mouse_map_x)*rme::TileSize - view_scroll_x - adjustment;
			int cy = (mouse_map_y)*rme::TileSize - view_scroll_y - adjustment;

			glColorCheck(brush, Position(mouse_map_x, mouse_map_y, floor));
			glBegin(GL_QUADS);
			glVertex2f(cx, cy + rme::TileSize);
			glVertex2f(cx + rme::TileSize, cy + rme::TileSize);
			glVertex2f(cx + rme::TileSize, cy);
			glVertex2f(cx, cy);
			glEnd();
		} else if (brush->isMonster()) {
			glEnable(GL_TEXTURE_2D);
			int cy = (mouse_map_y)*rme::TileSize - view_scroll_y - adjustment;
			int cx = (mouse_map_x)*rme::TileSize - view_scroll_x - adjustment;
			MonsterBrush* monster_brush = brush->asMonster();
			if (monster_brush->canDraw(&editor.getMap(), Position(mouse_map_x, mouse_map_y, floor))) {
				BlitCreature(cx, cy, monster_brush->getType()->outfit, SOUTH, 255, 255, 255, 160);
			} else {
				BlitCreature(cx, cy, monster_brush->getType()->outfit, SOUTH, 255, 64, 64, 160);
			}
			glDisable(GL_TEXTURE_2D);
		} else if (brush->isNpc()) {
			glEnable(GL_TEXTURE_2D);
			int cy = (mouse_map_y)*rme::TileSize - view_scroll_y - getFloorAdjustment(floor);
			int cx = (mouse_map_x)*rme::TileSize - view_scroll_x - getFloorAdjustment(floor);
			NpcBrush* npcBrush = brush->asNpc();
			if (npcBrush->canDraw(&editor.getMap(), Position(mouse_map_x, mouse_map_y, floor))) {
				BlitCreature(cx, cy, npcBrush->getType()->outfit, SOUTH, 255, 255, 255, 160);
			} else {
				BlitCreature(cx, cy, npcBrush->getType()->outfit, SOUTH, 255, 64, 64, 160);
			}
			glDisable(GL_TEXTURE_2D);
		} else if (!brush->isDoodad()) {
			RAWBrush* raw_brush = nullptr;
			if (brush->isRaw()) { // Textured brush
				glEnable(GL_TEXTURE_2D);
				raw_brush = brush->asRaw();
			} else {
				glDisable(GL_TEXTURE_2D);
			}

			for (int y = -g_gui.GetBrushSize() - 1; y <= g_gui.GetBrushSize() + 1; y++) {
				int cy = (mouse_map_y + y) * rme::TileSize - view_scroll_y - adjustment;
				for (int x = -g_gui.GetBrushSize() - 1; x <= g_gui.GetBrushSize() + 1; x++) {
					int cx = (mouse_map_x + x) * rme::TileSize - view_scroll_x - adjustment;
					if (g_gui.GetBrushShape() == BRUSHSHAPE_SQUARE) {
						if (x >= -g_gui.GetBrushSize() && x <= g_gui.GetBrushSize() && y >= -g_gui.GetBrushSize() && y <= g_gui.GetBrushSize()) {
							if (brush->isRaw()) {
								BlitSpriteType(cx, cy, raw_brush->getItemType()->sprite, 160, 160, 160, 160);
							} else {
								if (brush->isWaypoint()) {
									uint8_t r, g, b;
									getColor(brush, Position(mouse_map_x + x, mouse_map_y + y, floor), r, g, b);
									DrawBrushIndicator(cx, cy, brush, r, g, b);
								} else {
									if (brush->isHouseExit() || brush->isOptionalBorder()) {
										glColorCheck(brush, Position(mouse_map_x + x, mouse_map_y + y, floor));
									} else {
										glColor(brushColor);
									}

									glBegin(GL_QUADS);
									glVertex2f(cx, cy + rme::TileSize);
									glVertex2f(cx + rme::TileSize, cy + rme::TileSize);
									glVertex2f(cx + rme::TileSize, cy);
									glVertex2f(cx, cy);
									glEnd();
								}
							}
						}
					} else if (g_gui.GetBrushShape() == BRUSHSHAPE_CIRCLE) {
						double distance = sqrt(double(x * x) + double(y * y));
						if (distance < g_gui.GetBrushSize() + 0.005) {
							if (brush->isRaw()) {
								BlitSpriteType(cx, cy, raw_brush->getItemType()->sprite, 160, 160, 160, 160);
							} else {
								if (brush->isWaypoint()) {
									uint8_t r, g, b;
									getColor(brush, Position(mouse_map_x + x, mouse_map_y + y, floor), r, g, b);
									DrawBrushIndicator(cx, cy, brush, r, g, b);
								} else {
									if (brush->isHouseExit() || brush->isOptionalBorder()) {
										glColorCheck(brush, Position(mouse_map_x + x, mouse_map_y + y, floor));
									} else {
										glColor(brushColor);
									}

									glBegin(GL_QUADS);
									glVertex2f(cx, cy + rme::TileSize);
									glVertex2f(cx + rme::TileSize, cy + rme::TileSize);
									glVertex2f(cx + rme::TileSize, cy);
									glVertex2f(cx, cy);
									glEnd();
								}
							}
						}
					}
				}
			}

			if (brush->isRaw()) { // Textured brush
				glDisable(GL_TEXTURE_2D);
			} else {
				glEnable(GL_TEXTURE_2D);
			}
		}
	}
}

void MapDrawer::BlitItem(int &draw_x, int &draw_y, const Tile* tile, const Item* item, bool ephemeral, int red, int green, int blue, int alpha) {
	const ItemType &type = g_items.getItemType(item->getID());
	if (type.id == 0) {
		glDisable(GL_TEXTURE_2D);
		glBlitSquare(draw_x, draw_y, *wxRED);
		glEnable(GL_TEXTURE_2D);
		return;
	}

	if (!options.ingame && !ephemeral && item->isSelected()) {
		red /= 2;
		blue /= 2;
		green /= 2;
	}

	// Ugly hacks. :)
	if (type.id == ITEM_STAIRS && !options.ingame) {
		glDisable(GL_TEXTURE_2D);
		glBlitSquare(draw_x, draw_y, red, green, 0, alpha / 3 * 2);
		glEnable(GL_TEXTURE_2D);
		return;
	} else if (type.id == ITEM_NOTHING_SPECIAL && !options.ingame) {
		glDisable(GL_TEXTURE_2D);
		glBlitSquare(draw_x, draw_y, red, 0, 0, alpha / 3 * 2);
		glEnable(GL_TEXTURE_2D);
		return;
	}

	if (type.isMetaItem()) {
		return;
	}
	if (!ephemeral && type.pickupable && !options.show_items) {
		return;
	}
	GameSprite* sprite = type.sprite;
	if (!sprite) {
		return;
	}

	int screenx = draw_x - sprite->getDrawOffset().x;
	int screeny = draw_y - sprite->getDrawOffset().y;

	const Position &pos = tile->getPosition();

	// Set the newd drawing height accordingly
	draw_x -= sprite->getDrawHeight();
	draw_y -= sprite->getDrawHeight();

	int subtype = -1;

	int pattern_x = pos.x % sprite->pattern_x;
	int pattern_y = pos.y % sprite->pattern_y;
	int pattern_z = pos.z % sprite->pattern_z;

	if (type.isSplash() || type.isFluidContainer()) {
		subtype = Item::liquidSubTypeToSpriteSubType(item->getSubtype());
	} else if (type.isHangable) {
		if (tile->hasProperty(HOOK_SOUTH)) {
			pattern_x = 1;
		} else if (tile->hasProperty(HOOK_EAST)) {
			pattern_x = 2;
		} else {
			pattern_x = 0;
		}
	} else if (type.stackable) {
		if (item->getSubtype() <= 1) {
			subtype = 0;
		} else if (item->getSubtype() <= 2) {
			subtype = 1;
		} else if (item->getSubtype() <= 3) {
			subtype = 2;
		} else if (item->getSubtype() <= 4) {
			subtype = 3;
		} else if (item->getSubtype() < 10) {
			subtype = 4;
		} else if (item->getSubtype() < 25) {
			subtype = 5;
		} else if (item->getSubtype() < 50) {
			subtype = 6;
		} else {
			subtype = 7;
		}
	}

	if (!ephemeral && options.transparent_items && (!type.isGroundTile() || sprite->getWidth() > 1 || sprite->getHeight() > 1) && !type.isSplash() && (!type.isBorder || sprite->getWidth() > 1 || sprite->getHeight() > 1)) {
		alpha /= 2;
	}

	int frame = item->getFrame();
	int texnum = sprite->getHardwareID(0, subtype, pattern_x, pattern_y, pattern_z, frame);
	glBlitTexture(screenx, screeny, texnum, red, green, blue, alpha);

	if (options.show_hooks && (type.hookSouth || type.hookEast || type.hook != ITEM_HOOK_NONE)) {
		DrawHookIndicator(draw_x, draw_y, type);
	}

	if (!options.ingame && options.show_light_strength) {
		DrawLightStrength(draw_x, draw_y, item);
	}
}

void MapDrawer::BlitItem(int &draw_x, int &draw_y, const Position &pos, const Item* item, bool ephemeral, int red, int green, int blue, int alpha) {
	const ItemType &type = g_items.getItemType(item->getID());
	if (type.id == 0) {
		return;
	}

	if (!options.ingame && !ephemeral && item->isSelected()) {
		red /= 2;
		blue /= 2;
		green /= 2;
	}

	if (type.id == ITEM_STAIRS && !options.ingame) { // Ugly hack yes?
		glDisable(GL_TEXTURE_2D);
		glBlitSquare(draw_x, draw_y, red, green, 0, alpha / 3 * 2);
		glEnable(GL_TEXTURE_2D);
		return;
	} else if (type.id == ITEM_NOTHING_SPECIAL && !options.ingame) { // Ugly hack yes?
		glDisable(GL_TEXTURE_2D);
		glBlitSquare(draw_x, draw_y, red, 0, 0, alpha / 3 * 2);
		glEnable(GL_TEXTURE_2D);
		return;
	}

	if (type.isMetaItem()) {
		return;
	}
	if (!ephemeral && type.pickupable && options.show_items) {
		return;
	}
	GameSprite* sprite = type.sprite;
	if (!sprite) {
		return;
	}

	int screenx = draw_x - sprite->getDrawOffset().x;
	int screeny = draw_y - sprite->getDrawOffset().y;

	// Set the newd drawing height accordingly
	draw_x -= sprite->getDrawHeight();
	draw_y -= sprite->getDrawHeight();

	int subtype = -1;

	int pattern_x = pos.x % sprite->pattern_x;
	int pattern_y = pos.y % sprite->pattern_y;
	int pattern_z = pos.z % sprite->pattern_z;

	if (type.isSplash() || type.isFluidContainer()) {
		subtype = item->getSubtype();
	} else if (type.isHangable) {
		pattern_x = 0;
		/*
		if(tile->hasProperty(HOOK_SOUTH)) {
			pattern_x = 2;
		} else if(tile->hasProperty(HOOK_EAST)) {
			pattern_x = 1;
		} else {
			pattern_x = -0;
		}
		*/
	} else if (type.stackable) {
		if (item->getSubtype() <= 1) {
			subtype = 0;
		} else if (item->getSubtype() <= 2) {
			subtype = 1;
		} else if (item->getSubtype() <= 3) {
			subtype = 2;
		} else if (item->getSubtype() <= 4) {
			subtype = 3;
		} else if (item->getSubtype() < 10) {
			subtype = 4;
		} else if (item->getSubtype() < 25) {
			subtype = 5;
		} else if (item->getSubtype() < 50) {
			subtype = 6;
		} else {
			subtype = 7;
		}
	}

	if (!ephemeral && options.transparent_items && (!type.isGroundTile() || sprite->getWidth() > 1 || sprite->getHeight() > 1) && !type.isSplash() && (!type.isBorder || sprite->getWidth() > 1 || sprite->getHeight() > 1)) {
		alpha /= 2;
	}

	int frame = item->getFrame();
	int texnum = sprite->getHardwareID(0, subtype, pattern_x, pattern_y, pattern_z, frame);
	glBlitTexture(screenx, screeny, texnum, red, green, blue, alpha);

	if (options.show_hooks && (type.hookSouth || type.hookEast) && zoom <= 3.0) {
		DrawHookIndicator(draw_x, draw_y, type);
	}

	if (!options.ingame && options.show_light_strength) {
		DrawLightStrength(draw_x, draw_y, item);
	}
}

void MapDrawer::BlitSpriteType(int screenx, int screeny, uint32_t spriteid, int red, int green, int blue, int alpha) {
	const ItemType &type = g_items.getItemType(spriteid);
	if (type.id == 0) {
		return;
	}

	GameSprite* sprite = type.sprite;
	if (!sprite) {
		return;
	}

	screenx -= sprite->getDrawOffset().x;
	screeny -= sprite->getDrawOffset().y;

	int frame = 0;
	int texnum = sprite->getHardwareID(0, -1, 0, 0, 0, 0);
	glBlitTexture(screenx, screeny, texnum, red, green, blue, alpha);
}

void MapDrawer::BlitSpriteType(int screenx, int screeny, GameSprite* sprite, int red, int green, int blue, int alpha) {
	if (!sprite) {
		return;
	}

	screenx -= sprite->getDrawOffset().x;
	screeny -= sprite->getDrawOffset().y;

	int frame = 0;
	int texnum = sprite->getHardwareID(0, -1, 0, 0, 0, 0);
	glBlitTexture(screenx, screeny, texnum, red, green, blue, alpha);
}

void MapDrawer::BlitCreature(int screenx, int screeny, const Outfit &outfit, const Direction &dir, int red, int green, int blue, int alpha) {
	if (outfit.lookItem != 0) {
		const ItemType &type = g_items.getItemType(outfit.lookItem);
		BlitSpriteType(screenx, screeny, type.sprite, red, green, blue, alpha);
		return;
	}

	if (outfit.lookType == 0) {
		return;
	}

	GameSprite* spr = g_gui.gfx.getCreatureSprite(outfit.lookType);
	if (!spr || outfit.lookType == 0) {
		return;
	}

	screenx -= spr->getDrawOffset().x;
	screeny -= spr->getDrawOffset().y;

	auto spriteId = spr->spriteList[0]->getHardwareID();
	auto outfitImage = spr->getOutfitImage(spriteId, dir, outfit);
	if (outfitImage) {
		glBlitTexture(screenx, screeny, outfitImage->getHardwareID(), red, green, blue, alpha, false, false, outfit, spriteId);
	}
}

void MapDrawer::BlitCreature(int screenx, int screeny, const Monster* creature, int red, int green, int blue, int alpha) {
	if (!options.ingame && creature->isSelected()) {
		red /= 2;
		green /= 2;
		blue /= 2;
	}
	BlitCreature(screenx, screeny, creature->getLookType(), creature->getDirection(), red, green, blue, alpha);
}
// Npcs
void MapDrawer::BlitCreature(int screenx, int screeny, const Npc* npc, int red, int green, int blue, int alpha) {
	if (!options.ingame && npc->isSelected()) {
		red /= 2;
		green /= 2;
		blue /= 2;
	}
	BlitCreature(screenx, screeny, npc->getLookType(), npc->getDirection(), red, green, blue, alpha);
}

void MapDrawer::WriteTooltip(const Item* item, std::ostringstream &stream) {
	if (!item) {
		return;
	}

	const uint16_t id = item->getID();
	if (id < 100) {
		return;
	}

	const uint16_t unique = item->getUniqueID();
	const uint16_t action = item->getActionID();
	const std::string &text = item->getText();
	if (unique == 0 && action == 0 && text.empty()) {
		return;
	}

	if (stream.tellp() > 0) {
		stream << "\n";
	}

	stream << "id: " << id << "\n";

	if (action > 0) {
		stream << "aid: " << action << "\n";
	}
	if (unique > 0) {
		stream << "uid: " << unique << "\n";
	}
	if (!text.empty()) {
		stream << "text: " << text << "\n";
	}
}

void MapDrawer::WriteTooltip(const Waypoint* waypoint, std::ostringstream &stream) {
	if (stream.tellp() > 0) {
		stream << "\n";
	}
	stream << "wp: " << waypoint->name << "\n";
}

void MapDrawer::DrawTile(TileLocation* location) {
	if (!location) {
		return;
	}

	Tile* tile = location->get();
	if (!tile) {
		return;
	}

	if (options.show_only_modified && !tile->isModified()) {
		return;
	}

	const Position &position = location->getPosition();
	bool show_tooltips = options.isTooltips();

	if (show_tooltips && location->getWaypointCount() > 0) {
		Waypoint* waypoint = canvas->editor.getMap().waypoints.getWaypoint(position);
		if (waypoint) {
			WriteTooltip(waypoint, tooltip);
		}
	}

	bool only_colors = options.isOnlyColors();

	int draw_x, draw_y;
	getDrawPosition(position, draw_x, draw_y);

	uint8_t r = 255, g = 255, b = 255;
	if (only_colors || tile->hasGround()) {

		if (!options.show_as_minimap) {
			bool showspecial = options.show_only_colors || options.show_special_tiles;

			if (options.show_blocking && tile->isBlocking() && tile->size() > 0) {
				g = g / 3 * 2;
				b = b / 3 * 2;
			}

			int item_count = tile->items.size();
			if (options.highlight_items && item_count > 0 && !tile->items.back()->isBorder()) {
				static const float factor[5] = { 0.75f, 0.6f, 0.48f, 0.40f, 0.33f };
				int idx = (item_count < 5 ? item_count : 5) - 1;
				g = int(g * factor[idx]);
				r = int(r * factor[idx]);
			}

			if (options.show_spawns_monster && location->getSpawnMonsterCount() > 0) {
				float f = 1.0f;
				for (uint32_t i = 0; i < location->getSpawnMonsterCount(); ++i) {
					f *= 0.7f;
				}
				g = uint8_t(g * f);
				b = uint8_t(b * f);
			}

			if (options.show_spawns_npc && location->getSpawnNpcCount() > 0) {
				float f = 1.0f;
				for (uint32_t i = 0; i < location->getSpawnNpcCount(); ++i) {
					f *= 0.7f;
				}
				g = uint8_t(g * f);
				b = uint8_t(b * f);
			}

			if (options.show_houses && tile->isHouseTile()) {
				if ((int)tile->getHouseID() == current_house_id) {
					r /= 2;
				} else {
					r /= 2;
					g /= 2;
				}
			} else if (showspecial && tile->isPZ()) {
				r /= 2;
				b /= 2;
			}

			if (showspecial && tile->getMapFlags() & TILESTATE_PVPZONE) {
				g = r / 4;
				b = b / 3 * 2;
			}

			bool zone_active = tile->hasZone(g_gui.zone_brush->getZone());
			if (showspecial && zone_active) {
				b /= 1.3;
				r /= 1.5;
				g /= 2;
			}
			if (showspecial && ((!tile->zones.empty() && !zone_active) || tile->zones.size() > 1)) {
				r /= 1.4;
				g /= 1.6;
				b /= 1.3;
			}

			if (showspecial && tile->getMapFlags() & TILESTATE_NOLOGOUT) {
				b /= 2;
			}

			if (showspecial && tile->getMapFlags() & TILESTATE_NOPVP) {
				g /= 2;
			}
		}

		if (only_colors) {
			glDisable(GL_TEXTURE_2D);
			if (options.show_as_minimap) {
				wxColor color = colorFromEightBit(tile->getMiniMapColor());
				glBlitSquare(draw_x, draw_y, color);
			} else if (r != 255 || g != 255 || b != 255) {
				glBlitSquare(draw_x, draw_y, r, g, b, 128);
			}
			glEnable(GL_TEXTURE_2D);
		} else {
			if (options.show_preview && zoom <= 2.0) {
				tile->ground->animate();
			}

			BlitItem(draw_x, draw_y, tile, tile->ground, false, r, g, b);
		}

		if (show_tooltips && position.z == floor) {
			WriteTooltip(tile->ground, tooltip);
		}
	}

	bool hidden = only_colors || (options.hide_items_when_zoomed && zoom > 10.f);

	if (!hidden && !tile->items.empty()) {
		for (Item* item : tile->items) {
			if (show_tooltips && position.z == floor) {
				WriteTooltip(item, tooltip);
			}

			if (options.show_preview && zoom <= 2.0) {
				item->animate();
			}

			if (item->isBorder()) {
				BlitItem(draw_x, draw_y, tile, item, false, r, g, b);
			} else {
				BlitItem(draw_x, draw_y, tile, item);
			}
		}
	}

	if (!hidden && options.show_monsters && !tile->monsters.empty()) {
		for (auto monster : tile->monsters) {
			BlitCreature(draw_x, draw_y, monster);
		}
	}

	if (!hidden && options.show_npcs && tile->npc) {
		BlitCreature(draw_x, draw_y, tile->npc);
	}

	if (show_tooltips) {
		if (location->getWaypointCount() > 0) {
			MakeTooltip(draw_x, draw_y, tooltip.str(), 0, 255, 0);
		} else {
			MakeTooltip(draw_x, draw_y, tooltip.str());
		}
		tooltip.str("");
	}
}

void MapDrawer::DrawBrushIndicator(int x, int y, Brush* brush, uint8_t r, uint8_t g, uint8_t b) {
	x += (rme::TileSize / 2);
	y += (rme::TileSize / 2);

	// 7----0----1
	// |         |
	// 6--5  3--2
	//     \/
	//     4
	static int vertexes[9][2] = {
		{ -15, -20 }, // 0
		{ 15, -20 }, // 1
		{ 15, -5 }, // 2
		{ 5, -5 }, // 3
		{ 0, 0 }, // 4
		{ -5, -5 }, // 5
		{ -15, -5 }, // 6
		{ -15, -20 }, // 7
		{ -15, -20 }, // 0
	};

	// circle
	glBegin(GL_TRIANGLE_FAN);
	glColor4ub(0x00, 0x00, 0x00, 0x50);
	glVertex2i(x, y);
	for (int i = 0; i <= 30; i++) {
		float angle = i * 2.0f * rme::PI / 30;
		glVertex2f(cos(angle) * (rme::TileSize / 2) + x, sin(angle) * (rme::TileSize / 2) + y);
	}
	glEnd();

	// background
	glColor4ub(r, g, b, 0xB4);
	glBegin(GL_POLYGON);
	for (int i = 0; i < 8; ++i) {
		glVertex2i(vertexes[i][0] + x, vertexes[i][1] + y);
	}
	glEnd();

	// borders
	glColor4ub(0x00, 0x00, 0x00, 0xB4);
	glLineWidth(1.0);
	glBegin(GL_LINES);
	for (int i = 0; i < 8; ++i) {
		glVertex2i(vertexes[i][0] + x, vertexes[i][1] + y);
		glVertex2i(vertexes[i + 1][0] + x, vertexes[i + 1][1] + y);
	}
	glEnd();
}

void MapDrawer::DrawHookIndicator(int x, int y, const ItemType &type) {
	glDisable(GL_TEXTURE_2D);
	glColor4ub(uint8_t(0), uint8_t(0), uint8_t(255), uint8_t(200));
	glBegin(GL_QUADS);
	if (type.hookSouth || type.hook == ITEM_HOOK_SOUTH) {
		x -= 10;
		y += 10;
		glVertex2f(x, y);
		glVertex2f(x + 10, y);
		glVertex2f(x + 20, y + 10);
		glVertex2f(x + 10, y + 10);
	} else if (type.hookEast || type.hook == ITEM_HOOK_EAST) {
		x += 10;
		y -= 10;
		glVertex2f(x, y);
		glVertex2f(x + 10, y + 10);
		glVertex2f(x + 10, y + 20);
		glVertex2f(x, y + 10);
	}
	glEnd();
	glEnable(GL_TEXTURE_2D);
}

void MapDrawer::DrawLightStrength(int x, int y, const Item*&item) {
	const SpriteLight &light = item->getLight();

	if (light.intensity <= 0) {
		return;
	}

	wxColor lightColor = colorFromEightBit(light.color);
	const uint8_t byteR = lightColor.Red();
	const uint8_t byteG = lightColor.Green();
	const uint8_t byteB = lightColor.Blue();
	constexpr uint8_t byteA = 255;

	const int startOffset = std::max<int>(16, 32 - light.intensity);
	const int sqSize = rme::TileSize - startOffset;
	glDisable(GL_TEXTURE_2D);
	glBlitSquare(x + startOffset - 2, y + startOffset - 2, 0, 0, 0, byteA, sqSize + 2);
	glBlitSquare(x + startOffset - 1, y + startOffset - 1, byteR, byteG, byteB, byteA, sqSize);
	glEnable(GL_TEXTURE_2D);
}

void MapDrawer::DrawTileIndicators(TileLocation* location) {
	if (!location) {
		return;
	}

	Tile* tile = location->get();
	if (!tile) {
		return;
	}

	int x, y;
	getDrawPosition(location->getPosition(), x, y);

	if (zoom < 10.0 && (options.show_pickupables || options.show_moveables || options.show_avoidables)) {
		uint8_t red = 0xFF, green = 0xFF, blue = 0xFF;
		if (tile->isHouseTile()) {
			green = 0x00;
			blue = 0x00;
		}

		for (const Item* item : tile->items) {
			const ItemType &type = g_items.getItemType(item->getID());
			if ((type.pickupable && options.show_pickupables) || (type.moveable && options.show_moveables)) {
				if (type.pickupable && options.show_pickupables && type.moveable && options.show_moveables) {
					DrawIndicator(x, y, EDITOR_SPRITE_PICKUPABLE_MOVEABLE_ITEM, red, green, blue);
				} else if (type.pickupable && options.show_pickupables) {
					DrawIndicator(x, y, EDITOR_SPRITE_PICKUPABLE_ITEM, red, green, blue);
				} else if (type.moveable && options.show_moveables) {
					DrawIndicator(x, y, EDITOR_SPRITE_MOVEABLE_ITEM, red, green, blue);
				}
			}

			if (type.blockPathfinder && options.show_avoidables) {
				DrawIndicator(x, y, EDITOR_SPRITE_AVOIDABLE_ITEM, red, green, blue);
			}
		}
	}

	if (options.show_avoidables && tile->ground && tile->ground->isAvoidable()) {
		DrawIndicator(x, y, EDITOR_SPRITE_AVOIDABLE_ITEM);
	}

	if (options.show_houses && tile->isHouseExit()) {
		if (tile->hasHouseExit(current_house_id)) {
			DrawIndicator(x, y, EDITOR_SPRITE_HOUSE_EXIT);
		} else {
			DrawIndicator(x, y, EDITOR_SPRITE_HOUSE_EXIT, 64, 64, 255, 128);
		}
	}

	if (options.show_spawns_monster && tile->spawnMonster) {
		if (tile->spawnMonster->isSelected()) {
			DrawIndicator(x, y, EDITOR_SPRITE_MONSTERS, 128, 128, 128);
		} else {
			DrawIndicator(x, y, EDITOR_SPRITE_MONSTERS);
		}
	}

	if (tile->spawnNpc && options.show_spawns_npc) {
		if (tile->spawnNpc->isSelected()) {
			DrawIndicator(x, y, EDITOR_SPRITE_NPCS, 128, 128, 128);
		} else {
			DrawIndicator(x, y, EDITOR_SPRITE_NPCS, 255, 255, 255);
		}
	}
}

void MapDrawer::DrawIndicator(int x, int y, int indicator, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
	GameSprite* sprite = g_gui.gfx.getEditorSprite(indicator);
	if (sprite == nullptr) {
		spdlog::error("MapDrawer::DrawIndicator: sprite is nullptr");
		return;
	}

	int textureId = sprite->getHardwareID(0, 0, 0, -1, 0, 0);
	glBlitTexture(x, y, textureId, r, g, b, a, true, true);
}

void MapDrawer::DrawPositionIndicator(int z) {
	if (z != pos_indicator.z || pos_indicator.x < start_x || pos_indicator.x > end_x || pos_indicator.y < start_y || pos_indicator.y > end_y) {
		return;
	}

	const long time = GetPositionIndicatorTime();
	if (time == 0) {
		return;
	}

	int x, y;
	getDrawPosition(pos_indicator, x, y);

	int size = static_cast<int>(rme::TileSize * (0.3f + std::abs(500 - time % 1000) / 1000.f));
	int offset = (rme::TileSize - size) / 2;

	glDisable(GL_TEXTURE_2D);
	drawRect(x + offset + 2, y + offset + 2, size - 4, size - 4, *wxWHITE, 2);
	drawRect(x + offset + 1, y + offset + 1, size - 2, size - 2, *wxBLACK, 2);
	glEnable(GL_TEXTURE_2D);
}

void MapDrawer::DrawTooltips() {
#if defined(__LINUX__) || defined(__WINDOWS__)
	if (!options.show_tooltips || tooltips.empty()) {
		return;
	}

	glDisable(GL_TEXTURE_2D);

	for (MapTooltip* tooltip : tooltips) {
		const char* text = tooltip->text.c_str();
		float line_width = 0.0f;
		float width = 2.0f;
		float height = 14.0f;
		int char_count = 0;
		int line_char_count = 0;

		for (const char* c = text; *c != '\0'; c++) {
			if (*c == '\n' || (line_char_count >= MapTooltip::MAX_CHARS_PER_LINE && *c == ' ')) {
				height += 14.0f;
				line_width = 0.0f;
				line_char_count = 0;
			} else {
				line_width += glutBitmapWidth(GLUT_BITMAP_HELVETICA_12, *c);
			}
			width = std::max<float>(width, line_width);
			char_count++;
			line_char_count++;

			if (tooltip->ellipsis && char_count > (MapTooltip::MAX_CHARS + 3)) {
				break;
			}
		}

		float scale = zoom < 1.0f ? zoom : 1.0f;

		width = (width + 8.0f) * scale;
		height = (height + 4.0f) * scale;

		float x = tooltip->x + (rme::TileSize / 2.0f);
		float y = tooltip->y + ((rme::TileSize / 2.0f) * scale);
		float center = width / 2.0f;
		float space = (7.0f * scale);
		float startx = x - center;
		float endx = x + center;
		float starty = y - (height + space);
		float endy = y - space;

		// 7----0----1
		// |         |
		// 6--5  3--2
		//     \/
		//     4
		float vertexes[9][2] = {
			{ x, starty }, // 0
			{ endx, starty }, // 1
			{ endx, endy }, // 2
			{ x + space, endy }, // 3
			{ x, y }, // 4
			{ x - space, endy }, // 5
			{ startx, endy }, // 6
			{ startx, starty }, // 7
			{ x, starty }, // 0
		};

		// background
		glColor4ub(tooltip->r, tooltip->g, tooltip->b, 255);
		glBegin(GL_POLYGON);
		for (int i = 0; i < 8; ++i) {
			glVertex2f(vertexes[i][0], vertexes[i][1]);
		}
		glEnd();

		// borders
		glColor4ub(0, 0, 0, 255);
		glLineWidth(1.0);
		glBegin(GL_LINES);
		for (int i = 0; i < 8; ++i) {
			glVertex2f(vertexes[i][0], vertexes[i][1]);
			glVertex2f(vertexes[i + 1][0], vertexes[i + 1][1]);
		}
		glEnd();

		// text
		if (zoom <= 1.0) {
			startx += (3.0f * scale);
			starty += (14.0f * scale);
			glColor4ub(0, 0, 0, 255);
			glRasterPos2f(startx, starty);
			char_count = 0;
			line_char_count = 0;
			for (const char* c = text; *c != '\0'; c++) {
				if (*c == '\n' || (line_char_count >= MapTooltip::MAX_CHARS_PER_LINE && *c == ' ')) {
					starty += (14.0f * scale);
					glRasterPos2f(startx, starty);
					line_char_count = 0;
				}
				char_count++;
				line_char_count++;

				if (tooltip->ellipsis && char_count >= MapTooltip::MAX_CHARS) {
					glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, '.');
					if (char_count >= (MapTooltip::MAX_CHARS + 2)) {
						break;
					}
				} else if (!iscntrl(*c)) {
					glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);
				}
			}
		}
	}

	glEnable(GL_TEXTURE_2D);
#endif
}

void MapDrawer::DrawLight() const {
	// draw in-game light
	light_drawer->draw(start_x, start_y, end_x, end_y, view_scroll_x, view_scroll_y);
}

void MapDrawer::MakeTooltip(int screenx, int screeny, const std::string &text, uint8_t r, uint8_t g, uint8_t b) {
	if (text.empty()) {
		return;
	}

	MapTooltip* tooltip = new MapTooltip(screenx, screeny, text, r, g, b);
	tooltip->checkLineEnding();
	tooltips.push_back(tooltip);
}

void MapDrawer::AddLight(TileLocation* location) {
	if (!options.show_lights || !location) {
		return;
	}

	auto tile = location->get();
	if (!tile) {
		return;
	}

	auto &position = location->getPosition();

	if (tile->ground) {
		if (tile->ground->hasLight()) {
			light_drawer->addLight(position.x, position.y, position.z, tile->ground->getLight());
		}
	}

	bool hidden = options.hide_items_when_zoomed && zoom > 10.f;
	if (!hidden && !tile->items.empty()) {
		for (auto item : tile->items) {
			if (item->hasLight()) {
				light_drawer->addLight(position.x, position.y, position.z, item->getLight());
			}
		}
	}
}

void MapDrawer::getColor(Brush* brush, const Position &position, uint8_t &r, uint8_t &g, uint8_t &b) {
	if (brush->canDraw(&editor.getMap(), position)) {
		if (brush->isWaypoint()) {
			r = 0x00;
			g = 0xff, b = 0x00;
		} else {
			r = 0x00;
			g = 0x00, b = 0xff;
		}
	} else {
		r = 0xff;
		g = 0x00, b = 0x00;
	}
}

void MapDrawer::TakeScreenshot(uint8_t* screenshot_buffer) {
	glFinish(); // Wait for the operation to finish

	glPixelStorei(GL_PACK_ALIGNMENT, 1); // 1 byte alignment

	for (int i = 0; i < screensize_y; ++i) {
		glReadPixels(0, screensize_y - i, screensize_x, 1, GL_RGB, GL_UNSIGNED_BYTE, (GLubyte*)(screenshot_buffer) + 3 * screensize_x * i);
	}
}

void MapDrawer::ShowPositionIndicator(const Position &position) {
	pos_indicator = position;
	pos_indicator_timer.Start();
}

void MapDrawer::glBlitTexture(int sx, int sy, int textureId, int red, int green, int blue, int alpha, bool adjustZoom, bool isEditorSprite, const Outfit &outfit, int spriteId) {
	if (textureId <= 0) {
		return;
	}

	auto width = rme::TileSize;
	auto height = rme::TileSize;
	// Adjusts the offset of normal sprites
	if (!isEditorSprite) {
		SpriteSheetPtr sheet = g_spriteAppearances.getSheetBySpriteId(spriteId > 0 ? spriteId : textureId);
		if (!sheet) {
			return;
		}

		width = sheet->getSpriteSize().width;
		height = sheet->getSpriteSize().height;

		// If the sprite is an outfit and the size is 64x64, adjust the offset
		if (width == 64 && height == 64 && (outfit.lookType > 0 || outfit.lookItem > 0)) {
			GameSprite* spr = g_gui.gfx.getCreatureSprite(outfit.lookType);
			if (spr && spr->getDrawOffset().x == 8 && spr->getDrawOffset().y == 8) {
				sx -= width / 2;
				sy -= height / 2;
			}
		}
	}

	// Adjust zoom if necessary
	if (adjustZoom) {
		if (zoom < 1.0f) {
			float offset = 10 / (10 * zoom);
			width = std::max<int>(16, static_cast<int>(width * zoom));
			height = std::max<int>(16, static_cast<int>(height * zoom));
			sx += offset;
			sy += offset;
		} else if (zoom > 1.f) {
			float offset = (10 * zoom);
			width += static_cast<int>(offset);
			height += static_cast<int>(offset);
			sx -= offset;
			sy -= offset;
		}
	}

	if (outfit.lookType > 0) {
		spdlog::debug("Blitting outfit {} at ({}, {})", outfit.name, sx, sy);
	}

	glBindTexture(GL_TEXTURE_2D, textureId);
	glColor4ub(uint8_t(red), uint8_t(green), uint8_t(blue), uint8_t(alpha));
	glBegin(GL_QUADS);
	glTexCoord2f(0.f, 0.f);
	glVertex2f(sx, sy);
	glTexCoord2f(1.f, 0.f);
	glVertex2f(sx + width, sy);
	glTexCoord2f(1.f, 1.f);
	glVertex2f(sx + width, sy + height);
	glTexCoord2f(0.f, 1.f);
	glVertex2f(sx, sy + height);
	glEnd();
}

void MapDrawer::glBlitSquare(int x, int y, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha, int size /* = rme::TileSize */) const {
	const auto dx = static_cast<double>(x);
	const auto dy = static_cast<double>(y);
	const auto dSize = static_cast<double>(size);

	glColor4ub(red, green, blue, alpha);
	glBegin(GL_QUADS);
	glVertex2f(dx, dy);
	glVertex2f(dx + dSize, dy);
	glVertex2f(dx + dSize, dy + dSize);
	glVertex2f(dx, dy + dSize);
	glEnd();
}

void MapDrawer::glBlitSquare(int x, int y, const wxColor &color, int size /* = rme::TileSize */) const {
	const auto dx = static_cast<double>(x);
	const auto dy = static_cast<double>(y);
	const auto dSize = static_cast<double>(size);

	glColor4ub(color.Red(), color.Green(), color.Blue(), color.Alpha());
	glBegin(GL_QUADS);
	glVertex2f(dx, dy);
	glVertex2f(dx + dSize, dy);
	glVertex2f(dx + dSize, dy + dSize);
	glVertex2f(dx, dy + dSize);
	glEnd();
}

void MapDrawer::glColor(const wxColor &color) {
	glColor4ub(color.Red(), color.Green(), color.Blue(), color.Alpha());
}

void MapDrawer::glColor(MapDrawer::BrushColor color) {
	switch (color) {
		case COLOR_BRUSH:
			glColor4ub(
				g_settings.getInteger(Config::CURSOR_RED),
				g_settings.getInteger(Config::CURSOR_GREEN),
				g_settings.getInteger(Config::CURSOR_BLUE),
				g_settings.getInteger(Config::CURSOR_ALPHA)
			);
			break;

		case COLOR_FLAG_BRUSH:
		case COLOR_HOUSE_BRUSH:
			glColor4ub(
				g_settings.getInteger(Config::CURSOR_ALT_RED),
				g_settings.getInteger(Config::CURSOR_ALT_GREEN),
				g_settings.getInteger(Config::CURSOR_ALT_BLUE),
				g_settings.getInteger(Config::CURSOR_ALT_ALPHA)
			);
			break;

		case COLOR_SPAWN_BRUSH:
			glColor4ub(166, 0, 0, 128);
			break;

		case COLOR_SPAWN_NPC_BRUSH:
			glColor4ub(166, 0, 0, 128);
			break;

		case COLOR_ERASER:
			glColor4ub(166, 0, 0, 128);
			break;

		case COLOR_VALID:
			glColor4ub(0, 166, 0, 128);
			break;

		case COLOR_INVALID:
			glColor4ub(166, 0, 0, 128);
			break;

		default:
			glColor4ub(255, 255, 255, 128);
			break;
	}
}

void MapDrawer::glColorCheck(Brush* brush, const Position &pos) {
	if (brush->canDraw(&editor.getMap(), pos)) {
		glColor(COLOR_VALID);
	} else {
		glColor(COLOR_INVALID);
	}
}

void MapDrawer::drawRect(int x, int y, int w, int h, const wxColor &color, int width) {
	glLineWidth(width);
	glColor4ub(color.Red(), color.Green(), color.Blue(), color.Alpha());
	glBegin(GL_LINE_STRIP);
	glVertex2f(x, y);
	glVertex2f(x + w, y);
	glVertex2f(x + w, y + h);
	glVertex2f(x, y + h);
	glVertex2f(x, y);
	glEnd();
}

void MapDrawer::drawFilledRect(int x, int y, int w, int h, const wxColor &color) {
	glColor4ub(color.Red(), color.Green(), color.Blue(), color.Alpha());
	glBegin(GL_QUADS);
	glVertex2f(x, y);
	glVertex2f(x + w, y);
	glVertex2f(x + w, y + h);
	glVertex2f(x, y + h);
	glEnd();
}

void MapDrawer::getDrawPosition(const Position &position, int &x, int &y) {
	int offset;
	if (position.z <= rme::MapGroundLayer) {
		offset = (rme::MapGroundLayer - position.z) * rme::TileSize;
	} else {
		offset = rme::TileSize * (floor - position.z);
	}

	x = ((position.x * rme::TileSize) - view_scroll_x) - offset;
	y = ((position.y * rme::TileSize) - view_scroll_y) - offset;
}
