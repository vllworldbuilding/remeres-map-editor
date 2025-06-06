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

#ifndef RME_MAP_DRAWER_H_
#define RME_MAP_DRAWER_H_

class GameSprite;

struct MapTooltip {
	enum TextLength {
		MAX_CHARS_PER_LINE = 40,
		MAX_CHARS = 255,
	};

	MapTooltip(int x, int y, std::string text, uint8_t r, uint8_t g, uint8_t b) :
		x(x), y(y), text(text), r(r), g(g), b(b) {
		ellipsis = (text.length() - 3) > MAX_CHARS;
	}

	void checkLineEnding() {
		if (text.at(text.size() - 1) == '\n') {
			text.resize(text.size() - 1);
		}
	}

	int x, y;
	std::string text;
	uint8_t r, g, b;
	bool ellipsis;
};

// Storage during drawing, for option caching
struct DrawingOptions {
public:
	DrawingOptions();

	void SetIngame();
	void SetDefault();

	bool isOnlyColors() const noexcept;
	bool isTileIndicators() const noexcept;
	bool isTooltips() const noexcept;

	bool transparent_floors;
	bool transparent_items;
	bool show_ingame_box;
	bool show_light_strength;
	bool show_lights;
	bool ingame;
	bool dragging;

	int show_grid;
	bool show_all_floors;
	bool show_monsters;
	bool show_spawns_monster;
	bool show_npcs;
	bool show_spawns_npc;
	bool show_houses;
	bool show_shade;
	bool show_special_tiles;
	bool show_items;

	bool highlight_items;
	bool show_blocking;
	bool show_tooltips;
	bool show_as_minimap;
	bool show_only_colors;
	bool show_only_modified;
	bool show_preview;
	bool show_hooks;
	bool show_pickupables;
	bool show_moveables;
	bool show_avoidables;
	bool hide_items_when_zoomed;
};

class MapCanvas;
class LightDrawer;

class MapDrawer {
	MapCanvas* canvas;
	Editor &editor;
	DrawingOptions options;
	std::shared_ptr<LightDrawer> light_drawer;

	float zoom;

	uint32_t current_house_id;

	int mouse_map_x, mouse_map_y;
	int start_x, start_y, start_z;
	int end_x, end_y, end_z, superend_z;
	int view_scroll_x, view_scroll_y;
	int screensize_x, screensize_y;
	int tile_size;
	int floor;

protected:
	std::vector<MapTooltip*> tooltips;
	std::ostringstream tooltip;

	wxStopWatch pos_indicator_timer;
	Position pos_indicator;

public:
	MapDrawer(MapCanvas* canvas);
	~MapDrawer();

	bool dragging;
	bool dragging_draw;

	void SetupVars();
	void SetupGL();
	void Release();

	void Draw();
	void DrawBackground();
	void DrawShade(int mapz);
	void DrawMap();
	void DrawSecondaryMap(int mapz);
	void DrawDraggingShadow();
	void DrawHigherFloors();
	void DrawSelectionBox();
	void DrawLiveCursors();
	void DrawBrush();
	void DrawIngameBox();
	void DrawGrid();
	void DrawTooltips();

	void TakeScreenshot(uint8_t* screenshot_buffer);

	void ShowPositionIndicator(const Position &position);
	long GetPositionIndicatorTime() const {
		const long time = pos_indicator_timer.Time();
		if (time < rme::PositionIndicatorDuration) {
			return time;
		}
		return 0;
	}

	DrawingOptions &getOptions() noexcept {
		return options;
	}

protected:
	void BlitItem(int &screenx, int &screeny, const Tile* tile, const Item* item, bool ephemeral = false, int red = 255, int green = 255, int blue = 255, int alpha = 255);
	void BlitItem(int &screenx, int &screeny, const Position &pos, const Item* item, bool ephemeral = false, int red = 255, int green = 255, int blue = 255, int alpha = 255);
	void BlitSpriteType(int screenx, int screeny, uint32_t spriteid, int red = 255, int green = 255, int blue = 255, int alpha = 255);
	void BlitSpriteType(int screenx, int screeny, GameSprite* spr, int red = 255, int green = 255, int blue = 255, int alpha = 255);
	void BlitCreature(int screenx, int screeny, const Monster* npc, int red = 255, int green = 255, int blue = 255, int alpha = 255);
	void BlitCreature(int screenx, int screeny, const Npc* c, int red = 255, int green = 255, int blue = 255, int alpha = 255);
	void BlitCreature(int screenx, int screeny, const Outfit &outfit, const Direction &dir, int red = 255, int green = 255, int blue = 255, int alpha = 255);
	void DrawTile(TileLocation* tile);
	void DrawBrushIndicator(int x, int y, Brush* brush, uint8_t r, uint8_t g, uint8_t b);
	void DrawHookIndicator(int x, int y, const ItemType &type);
	void DrawLightStrength(int x, int y, const Item*&item);
	void DrawTileIndicators(TileLocation* location);
	void DrawIndicator(int x, int y, int indicator, uint8_t r = 255, uint8_t g = 255, uint8_t b = 255, uint8_t a = 255);
	void DrawPositionIndicator(int z);
	void DrawLight() const;
	void WriteTooltip(const Item* item, std::ostringstream &stream);
	void WriteTooltip(const Waypoint* item, std::ostringstream &stream);
	void MakeTooltip(int screenx, int screeny, const std::string &text, uint8_t r = 255, uint8_t g = 255, uint8_t b = 255);
	void AddLight(TileLocation* location);

	enum BrushColor {
		COLOR_BRUSH,
		COLOR_HOUSE_BRUSH,
		COLOR_FLAG_BRUSH,
		COLOR_SPAWN_BRUSH,
		COLOR_SPAWN_NPC_BRUSH,
		COLOR_ERASER,
		COLOR_VALID,
		COLOR_INVALID,
		COLOR_BLANK,
	};

	void getColor(Brush* brush, const Position &position, uint8_t &r, uint8_t &g, uint8_t &b);
	void glBlitTexture(int x, int y, int textureId, int red, int green, int blue, int alpha, bool adjustZoom = false, bool isEditorSprite = false, const Outfit &outfit = {}, int spriteId = 0);
	void glBlitSquare(int x, int y, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha, int size = rme::TileSize) const;
	void glBlitSquare(int x, int y, const wxColor &color, int size = rme::TileSize) const;
	void glColor(const wxColor &color);
	void glColor(BrushColor color);
	void glColorCheck(Brush* brush, const Position &pos);
	void drawRect(int x, int y, int w, int h, const wxColor &color, int width = 1);
	void drawFilledRect(int x, int y, int w, int h, const wxColor &color);

private:
	void getDrawPosition(const Position &position, int &x, int &y);
};

#endif
