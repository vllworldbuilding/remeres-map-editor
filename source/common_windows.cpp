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

#include "materials.h"
#include "brush.h"
#include "editor.h"

#include "items.h"
#include "map.h"
#include "item.h"
#include "complexitem.h"
#include "raw_brush.h"

#include "palette_window.h"
#include "gui.h"
#include "application.h"
#include "common_windows.h"
#include "positionctrl.h"
#include "preferences.h"

#include "iominimap.h"

#ifdef _MSC_VER
	#pragma warning(disable : 4018) // signed/unsigned mismatch
#endif

// ============================================================================
// Map Properties Window

BEGIN_EVENT_TABLE(MapPropertiesWindow, wxDialog)
EVT_CHOICE(MAP_PROPERTIES_VERSION, MapPropertiesWindow::OnChangeVersion)
EVT_BUTTON(wxID_OK, MapPropertiesWindow::OnClickOK)
EVT_BUTTON(wxID_CANCEL, MapPropertiesWindow::OnClickCancel)
END_EVENT_TABLE()

MapPropertiesWindow::MapPropertiesWindow(wxWindow* parent, MapTab* view, Editor &editor) :
	wxDialog(parent, wxID_ANY, "Map Properties", wxDefaultPosition, wxSize(300, 200), wxRESIZE_BORDER | wxCAPTION),
	view(view),
	editor(editor) {
	// Setup data variabels
	const Map &map = editor.getMap();

	wxSizer* topsizer = newd wxBoxSizer(wxVERTICAL);

	wxFlexGridSizer* grid_sizer = newd wxFlexGridSizer(2, 10, 10);
	grid_sizer->AddGrowableCol(1);

	// Description
	grid_sizer->Add(newd wxStaticText(this, wxID_ANY, "Map Description"));
	description_ctrl = newd wxTextCtrl(this, wxID_ANY, wxstr(map.getMapDescription()), wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE);
	grid_sizer->Add(description_ctrl, wxSizerFlags(1).Expand());

	// Map version
	grid_sizer->Add(newd wxStaticText(this, wxID_ANY, "Map Version"));
	version_choice = newd wxChoice(this, MAP_PROPERTIES_VERSION);
	version_choice->Append("0.5.0");
	version_choice->Append("0.6.0");
	version_choice->Append("0.6.1");
	version_choice->Append("0.7.0 (revscriptsys)");
	version_choice->Append("1.0.0");
	version_choice->Append("1.1.0 (revscriptsys)");

	switch (map.getVersion().otbm) {
		case MAP_OTBM_1:
			version_choice->SetSelection(0);
			break;
		case MAP_OTBM_2:
			version_choice->SetSelection(1);
			break;
		case MAP_OTBM_3:
			version_choice->SetSelection(2);
			break;
		case MAP_OTBM_4:
			version_choice->SetSelection(3);
			break;
		case MAP_OTBM_5:
			version_choice->SetSelection(4);
			break;
		case MAP_OTBM_6:
			version_choice->SetSelection(5);
			break;
		default:
			version_choice->SetSelection(0);
	}

	grid_sizer->Add(version_choice, wxSizerFlags(1).Expand());

	// Dimensions
	grid_sizer->Add(newd wxStaticText(this, wxID_ANY, "Map Dimensions"));
	{
		wxSizer* subsizer = newd wxBoxSizer(wxHORIZONTAL);
		subsizer->Add(
			width_spin = newd wxSpinCtrl(this, wxID_ANY, wxstr(i2s(map.getWidth())), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, rme::MapMinWidth, rme::MapMaxWidth), wxSizerFlags(1).Expand()
		);
		subsizer->Add(
			height_spin = newd wxSpinCtrl(this, wxID_ANY, wxstr(i2s(map.getHeight())), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, rme::MapMinHeight, rme::MapMaxHeight), wxSizerFlags(1).Expand()
		);
		grid_sizer->Add(subsizer, 1, wxEXPAND);
	}

	// External files
	grid_sizer->Add(
		newd wxStaticText(this, wxID_ANY, "External house file")
	);

	grid_sizer->Add(
		house_filename_ctrl = newd wxTextCtrl(this, wxID_ANY, wxstr(map.getHouseFilename())), 1, wxEXPAND
	);

	grid_sizer->Add(
		newd wxStaticText(this, wxID_ANY, "External zone file")
	);

	grid_sizer->Add(
		zone_filename_ctrl = newd wxTextCtrl(this, wxID_ANY, wxstr(map.getZoneFilename())), 1, wxEXPAND
	);

	grid_sizer->Add(
		newd wxStaticText(this, wxID_ANY, "External monster file")
	);

	grid_sizer->Add(
		spawn_filename_ctrl = newd wxTextCtrl(this, wxID_ANY, wxstr(map.getSpawnFilename())), 1, wxEXPAND
	);

	grid_sizer->Add(
		newd wxStaticText(this, wxID_ANY, "External npc file")
	);

	grid_sizer->Add(
		spawn_npc_filename_ctrl = newd wxTextCtrl(this, wxID_ANY, wxstr(map.getSpawnNpcFilename())), 1, wxEXPAND
	);

	topsizer->Add(grid_sizer, wxSizerFlags(1).Expand().Border(wxALL, 20));

	wxSizer* subsizer = newd wxBoxSizer(wxHORIZONTAL);
	subsizer->Add(newd wxButton(this, wxID_OK, "OK"), wxSizerFlags(1).Center());
	subsizer->Add(newd wxButton(this, wxID_CANCEL, "Cancel"), wxSizerFlags(1).Center());
	topsizer->Add(subsizer, wxSizerFlags(0).Center().Border(wxLEFT | wxRIGHT | wxBOTTOM, 20));

	SetSizerAndFit(topsizer);
	Centre(wxBOTH);
}

void MapPropertiesWindow::OnChangeVersion(wxCommandEvent &) {
	//
}

struct MapConversionContext {
	struct MonsterInfo {
		std::string name;
		Outfit outfit;
	};
	typedef std::map<std::string, MonsterInfo> MonsterMap;
	MonsterMap monsterType;

	struct NpcInfo {
		std::string name;
		Outfit outfit;
	};
	typedef std::map<std::string, NpcInfo> NpcMap;
	NpcMap npcType;

	void operator()(Map &map, Tile* tile, long long done) {
		for (const auto monster : tile->monsters) {
			const auto it = monsterType.find(monster->getName());
			if (it == monsterType.end()) {
				MonsterInfo monsterInfo = {
					monster->getName(),
					monster->getLookType()
				};
				monsterType[monster->getName()] = monsterInfo;
			}
		}
		if (tile->npc) {
			NpcMap::iterator f = npcType.find(tile->npc->getName());
			if (f == npcType.end()) {
				NpcInfo info = {
					tile->npc->getName(),
					tile->npc->getLookType()
				};
				npcType[tile->npc->getName()] = info;
			}
		}
	}
};

void MapPropertiesWindow::OnClickOK(wxCommandEvent &WXUNUSED(event)) {
	Map &map = editor.getMap();
	MapVersion old_ver = map.getVersion();
	MapVersion new_ver;

	wxString ver = version_choice->GetStringSelection();

	if (ver.Contains("0.5.0")) {
		new_ver.otbm = MAP_OTBM_1;
	} else if (ver.Contains("0.6.0")) {
		new_ver.otbm = MAP_OTBM_2;
	} else if (ver.Contains("0.6.1")) {
		new_ver.otbm = MAP_OTBM_3;
	} else if (ver.Contains("0.7.0")) {
		new_ver.otbm = MAP_OTBM_4;
	} else if (ver.Contains("1.0.0")) {
		new_ver.otbm = MAP_OTBM_5;
	} else if (ver.Contains("1.1.0")) {
		new_ver.otbm = MAP_OTBM_6;
	}

	if (new_ver.otbm != old_ver.otbm) {
		if (g_gui.GetOpenMapCount() > 1) {
			g_gui.PopupDialog(this, "Error", "You can not change editor version with multiple maps open", wxOK);
			return;
		}
		wxString error;
		wxArrayString warnings;

		// Switch version
		g_gui.GetCurrentEditor()->getSelection().clear();
		g_gui.GetCurrentEditor()->clearActions();

		if (new_ver.otbm < old_ver.otbm) {
			int ret = g_gui.PopupDialog(this, "Notice", "Converting to a previous version may have serious side-effects, are you sure you want to do this?", wxYES | wxNO);
			if (ret != wxID_YES) {
				return;
			}
			UnnamedRenderingLock();

			// Remember all monsters types on the map
			MapConversionContext conversion_context;
			foreach_TileOnMap(map, conversion_context);

			// Perform the conversion
			map.convert(new_ver, true);

			// Load the new version
			if (!g_gui.loadMapWindow(error, warnings)) {
				g_gui.ListDialog(this, "Warnings", warnings);
				g_gui.PopupDialog(this, "Map Loader Error", error, wxOK);
				g_gui.PopupDialog(this, "Conversion Error", "Could not convert map. The map will now be closed.", wxOK);

				auto clientDirectory = ClientAssets::getPath().ToStdString() + "/";
				if (clientDirectory.empty() || !wxDirExists(wxString(clientDirectory))) {
					PreferencesWindow dialog(nullptr);
					dialog.getBookCtrl().SetSelection(4);
					dialog.ShowModal();
					dialog.Destroy();
				}

				EndModal(0);
				return;
			}

			// Monsters
			// Remove all monsters that were present are present in the new version
			for (MapConversionContext::MonsterMap::iterator cs = conversion_context.monsterType.begin(); cs != conversion_context.monsterType.end();) {
				if (g_monsters[cs->first]) {
					cs = conversion_context.monsterType.erase(cs);
				} else {
					++cs;
				}
			}

			if (conversion_context.monsterType.size() > 0) {
				int add = g_gui.PopupDialog(this, "Unrecognized monsters", "There were monsters on the old version that are not present in this and were on the map, do you want to add them to this version as well?", wxYES | wxNO);
				if (add == wxID_YES) {
					for (MapConversionContext::MonsterMap::iterator cs = conversion_context.monsterType.begin(); cs != conversion_context.monsterType.end(); ++cs) {
						MapConversionContext::MonsterInfo info = cs->second;
						g_monsters.addMonsterType(info.name, info.outfit);
					}
				}
			}

			// Npcs
			// Remove all npcs that were present are present in the new version
			for (MapConversionContext::NpcMap::iterator cs = conversion_context.npcType.begin(); cs != conversion_context.npcType.end();) {
				if (g_npcs[cs->first]) {
					cs = conversion_context.npcType.erase(cs);
				} else {
					++cs;
				}
			}

			if (conversion_context.npcType.size() > 0) {
				int add = g_gui.PopupDialog(this, "Unrecognized npcs", "There were npcs on the old version that are not present in this and were on the map, do you want to add them to this version as well?", wxYES | wxNO);
				if (add == wxID_YES) {
					for (MapConversionContext::NpcMap::iterator cs = conversion_context.npcType.begin(); cs != conversion_context.npcType.end(); ++cs) {
						MapConversionContext::NpcInfo info = cs->second;
						g_npcs.addNpcType(info.name, info.outfit);
					}
				}
			}

			map.cleanInvalidTiles(true);
		} else {
			UnnamedRenderingLock();
			if (!g_gui.loadMapWindow(error, warnings)) {
				g_gui.ListDialog(this, "Warnings", warnings);
				g_gui.PopupDialog(this, "Map Loader Error", error, wxOK);
				g_gui.PopupDialog(this, "Conversion Error", "Could not convert map. The map will now be closed.", wxOK);

				auto clientDirectory = ClientAssets::getPath().ToStdString() + "/";
				if (clientDirectory.empty() || !wxDirExists(wxString(clientDirectory))) {
					PreferencesWindow dialog(nullptr);
					dialog.getBookCtrl().SetSelection(4);
					dialog.ShowModal();
					dialog.Destroy();
				}

				EndModal(0);
				return;
			}
			map.convert(new_ver, true);
		}
	} else {
		map.convert(new_ver, true);
	}

	map.setMapDescription(nstr(description_ctrl->GetValue()));
	map.setHouseFilename(nstr(house_filename_ctrl->GetValue()));
	map.setSpawnMonsterFilename(nstr(spawn_filename_ctrl->GetValue()));
	map.setSpawnNpcFilename(nstr(spawn_npc_filename_ctrl->GetValue()));
	map.setZoneFilename(nstr(zone_filename_ctrl->GetValue()));

	// Only resize if we have to
	int new_map_width = width_spin->GetValue();
	int new_map_height = height_spin->GetValue();
	if (new_map_width != map.getWidth() || new_map_height != map.getHeight()) {
		map.setWidth(new_map_width);
		map.setHeight(new_map_height);
		g_gui.FitViewToMap(view);
	}
	g_gui.RefreshPalettes();

	EndModal(1);
}

void MapPropertiesWindow::OnClickCancel(wxCommandEvent &WXUNUSED(event)) {
	// Just close this window
	EndModal(1);
}

MapPropertiesWindow::~MapPropertiesWindow() = default;

// ============================================================================
// Map Import Window

BEGIN_EVENT_TABLE(ImportMapWindow, wxDialog)
EVT_BUTTON(MAP_WINDOW_FILE_BUTTON, ImportMapWindow::OnClickBrowse)
EVT_BUTTON(wxID_OK, ImportMapWindow::OnClickOK)
EVT_BUTTON(wxID_CANCEL, ImportMapWindow::OnClickCancel)
END_EVENT_TABLE()

ImportMapWindow::ImportMapWindow(wxWindow* parent, Editor &editor) :
	wxDialog(parent, wxID_ANY, "Import Map", wxDefaultPosition, wxSize(420, 350)),
	editor(editor) {
	wxBoxSizer* sizer = newd wxBoxSizer(wxVERTICAL);
	wxStaticBoxSizer* tmpsizer;

	// File
	tmpsizer = newd wxStaticBoxSizer(new wxStaticBox(this, wxID_ANY, "Map File"), wxHORIZONTAL);
	file_text_field = newd wxTextCtrl(tmpsizer->GetStaticBox(), wxID_ANY, "", wxDefaultPosition, wxSize(300, 23));
	tmpsizer->Add(file_text_field, 0, wxALL, 5);
	wxButton* browse_button = newd wxButton(tmpsizer->GetStaticBox(), MAP_WINDOW_FILE_BUTTON, "Browse...", wxDefaultPosition, wxSize(80, 23));
	tmpsizer->Add(browse_button, 0, wxALL, 5);
	sizer->Add(tmpsizer, 1, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 5);

	// Import offset
	tmpsizer = newd wxStaticBoxSizer(new wxStaticBox(this, wxID_ANY, "Import Offset"), wxHORIZONTAL);
	tmpsizer->Add(newd wxStaticText(tmpsizer->GetStaticBox(), wxID_ANY, "Offset X:"), 0, wxALL | wxEXPAND, 5);
	x_offset_ctrl = newd wxSpinCtrl(tmpsizer->GetStaticBox(), wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(60, 23), wxSP_ARROW_KEYS, -rme::MapMaxHeight, rme::MapMaxHeight);
	tmpsizer->Add(x_offset_ctrl, 0, wxALL, 5);
	tmpsizer->Add(newd wxStaticText(tmpsizer->GetStaticBox(), wxID_ANY, "Offset Y:"), 0, wxALL, 5);
	y_offset_ctrl = newd wxSpinCtrl(tmpsizer->GetStaticBox(), wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(60, 23), wxSP_ARROW_KEYS, -rme::MapMaxHeight, rme::MapMaxHeight);
	tmpsizer->Add(y_offset_ctrl, 0, wxALL, 5);
	tmpsizer->Add(newd wxStaticText(tmpsizer->GetStaticBox(), wxID_ANY, "Offset Z:"), 0, wxALL, 5);
	z_offset_ctrl = newd wxSpinCtrl(tmpsizer->GetStaticBox(), wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(60, 23), wxSP_ARROW_KEYS, -rme::MapMaxLayer, rme::MapMaxLayer);
	tmpsizer->Add(z_offset_ctrl, 0, wxALL, 5);
	sizer->Add(tmpsizer, 1, wxEXPAND | wxLEFT | wxRIGHT, 5);

	// Import options
	wxArrayString house_choices;
	house_choices.Add("Smart Merge");
	house_choices.Add("Insert");
	house_choices.Add("Merge");
	house_choices.Add("Don't Import");

	// House options
	tmpsizer = newd wxStaticBoxSizer(new wxStaticBox(this, wxID_ANY, "House Import Behaviour"), wxVERTICAL);
	house_options = newd wxChoice(tmpsizer->GetStaticBox(), wxID_ANY, wxDefaultPosition, wxDefaultSize, house_choices);
	house_options->SetSelection(0);
	tmpsizer->Add(house_options, 0, wxALL | wxEXPAND, 5);
	sizer->Add(tmpsizer, 1, wxEXPAND | wxLEFT | wxRIGHT, 5);

	// Import options
	wxArrayString spawn_choices;
	spawn_choices.Add("Merge");
	spawn_choices.Add("Don't Import");

	// Monster spawn options
	tmpsizer = newd wxStaticBoxSizer(new wxStaticBox(this, wxID_ANY, "Monster Spawn Import Behaviour"), wxVERTICAL);
	spawn_monster_options = newd wxChoice(tmpsizer->GetStaticBox(), wxID_ANY, wxDefaultPosition, wxDefaultSize, spawn_choices);
	spawn_monster_options->SetSelection(0);
	tmpsizer->Add(spawn_monster_options, 0, wxALL | wxEXPAND, 5);
	sizer->Add(tmpsizer, 1, wxEXPAND | wxLEFT | wxRIGHT, 5);

	// Npc spawn options
	tmpsizer = newd wxStaticBoxSizer(new wxStaticBox(this, wxID_ANY, "Npc Spawn Import Behaviour"), wxVERTICAL);
	spawn_npc_options = newd wxChoice(tmpsizer->GetStaticBox(), wxID_ANY, wxDefaultPosition, wxDefaultSize, spawn_choices);
	spawn_npc_options->SetSelection(0);
	tmpsizer->Add(spawn_npc_options, 0, wxALL | wxEXPAND, 5);
	sizer->Add(tmpsizer, 1, wxEXPAND | wxLEFT | wxRIGHT, 5);

	// OK/Cancel buttons
	wxBoxSizer* buttons = newd wxBoxSizer(wxHORIZONTAL);
	buttons->Add(newd wxButton(this, wxID_OK, "Ok"), 0, wxALL, 5);
	buttons->Add(newd wxButton(this, wxID_CANCEL, "Cancel"), 0, wxALL, 5);
	sizer->Add(buttons, wxSizerFlags(1).Center());

	SetSizer(sizer);
	Layout();
	Centre(wxBOTH);
}

ImportMapWindow::~ImportMapWindow() = default;

void ImportMapWindow::OnClickBrowse(wxCommandEvent &WXUNUSED(event)) {
	wxFileDialog dialog(this, "Import...", "", "", "*.otbm", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
	int ok = dialog.ShowModal();

	if (ok == wxID_OK) {
		file_text_field->ChangeValue(dialog.GetPath());
	}
}

void ImportMapWindow::OnClickOK(wxCommandEvent &WXUNUSED(event)) {
	if (Validate() && TransferDataFromWindow()) {
		wxFileName fn = file_text_field->GetValue();
		if (!fn.FileExists()) {
			g_gui.PopupDialog(this, "Error", "The specified map file doesn't exist", wxOK);
			return;
		}

		ImportType spawn_import_type = IMPORT_DONT;
		ImportType spawn_npc_import_type = IMPORT_DONT;
		ImportType house_import_type = IMPORT_DONT;

		switch (spawn_monster_options->GetSelection()) {
			case 0:
				spawn_import_type = IMPORT_MERGE;
				break;
			case 1:
				spawn_import_type = IMPORT_DONT;
				break;
		}

		switch (spawn_npc_options->GetSelection()) {
			case 0:
				spawn_npc_import_type = IMPORT_MERGE;
				break;
			case 1:
				spawn_npc_import_type = IMPORT_DONT;
				break;
		}

		switch (house_options->GetSelection()) {
			case 0:
				house_import_type = IMPORT_SMART_MERGE;
				break;
			case 1:
				house_import_type = IMPORT_MERGE;
				break;
			case 2:
				house_import_type = IMPORT_INSERT;
				break;
			case 3:
				house_import_type = IMPORT_DONT;
				break;
		}

		EndModal(1);

		editor.importMap(fn, x_offset_ctrl->GetValue(), y_offset_ctrl->GetValue(), z_offset_ctrl->GetValue(), house_import_type, spawn_import_type, spawn_npc_import_type);
	}
}

void ImportMapWindow::OnClickCancel(wxCommandEvent &WXUNUSED(event)) {
	// Just close this window
	EndModal(0);
}

// ============================================================================
// Export Minimap window

BEGIN_EVENT_TABLE(ExportMiniMapWindow, wxDialog)
EVT_BUTTON(MAP_WINDOW_FILE_BUTTON, ExportMiniMapWindow::OnClickBrowse)
EVT_BUTTON(wxID_OK, ExportMiniMapWindow::OnClickOK)
EVT_BUTTON(wxID_CANCEL, ExportMiniMapWindow::OnClickCancel)
EVT_CHOICE(wxID_ANY, ExportMiniMapWindow::OnExportTypeChange)
END_EVENT_TABLE()

ExportMiniMapWindow::ExportMiniMapWindow(wxWindow* parent, Editor &editor) :
	wxDialog(parent, wxID_ANY, "Export Minimap", wxDefaultPosition, wxSize(400, 350)),
	editor(editor) {
	wxSizer* sizer = newd wxBoxSizer(wxVERTICAL);
	wxSizer* tmpsizer;

	// Error field
	error_field = newd wxStaticText(this, wxID_VIEW_DETAILS, "", wxDefaultPosition, wxDefaultSize);
	error_field->SetForegroundColour(*wxRED);
	tmpsizer = newd wxBoxSizer(wxHORIZONTAL);
	tmpsizer->Add(error_field, 0, wxALL, 5);
	sizer->Add(tmpsizer, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 5);

	// Output folder
	directory_text_field = newd wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize);
	directory_text_field->Bind(wxEVT_KEY_UP, &ExportMiniMapWindow::OnDirectoryChanged, this);
	directory_text_field->SetValue(wxString(g_settings.getString(Config::MINIMAP_EXPORT_DIR)));
	tmpsizer = newd wxStaticBoxSizer(wxHORIZONTAL, this, "Output Folder");
	tmpsizer->Add(directory_text_field, 1, wxALL, 5);
	tmpsizer->Add(newd wxButton(this, MAP_WINDOW_FILE_BUTTON, "Browse"), 0, wxALL, 5);
	sizer->Add(tmpsizer, 0, wxALL | wxEXPAND, 5);

	// Format options
	wxArrayString imageFormatChoices;
	imageFormatChoices.Add("1024x1024");
	imageFormatChoices.Add("2048x2048");
	imageFormatChoices.Add("4096x4096");
	imageSizeOptions = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, imageFormatChoices);
	imageSizeOptions->SetSelection(0);
	imageSizeOptions->Disable();
	tmpsizer = newd wxStaticBoxSizer(wxHORIZONTAL, this, "Image Size");
	tmpsizer->Add(imageSizeOptions, 1, wxALL, 5);
	sizer->Add(tmpsizer, 0, wxALL | wxEXPAND, 5);

	// File name
	wxString mapName(editor.getMap().getName().c_str(), wxConvUTF8);
	file_name_text_field = newd wxTextCtrl(this, wxID_ANY, mapName.BeforeLast('.'), wxDefaultPosition, wxDefaultSize);
	file_name_text_field->Bind(wxEVT_KEY_UP, &ExportMiniMapWindow::OnFileNameChanged, this);
	tmpsizer = newd wxStaticBoxSizer(wxHORIZONTAL, this, "File Name");
	tmpsizer->Add(file_name_text_field, 1, wxALL, 5);
	sizer->Add(tmpsizer, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 5);

	// Format options
	wxArrayString format_choices;
	format_choices.Add(".otmm (Client Minimap)");
	format_choices.Add(".png (PNG Image)");
	format_choices.Add(".bmp (Bitmap Image)");
	format_options = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, format_choices);
	format_options->SetSelection(0);
	tmpsizer->Add(format_options, 1, wxALL, 5);

	// Export options
	wxArrayString choices;
	choices.Add("All Floors");
	choices.Add("Ground Floor");
	choices.Add("Specific Floor");

	if (editor.hasSelection()) {
		choices.Add("Selected Area");
	}

	// Area options
	tmpsizer = newd wxStaticBoxSizer(wxHORIZONTAL, this, "Area Options");
	floor_options = newd wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, choices);
	floor_number = newd wxSpinCtrl(this, wxID_ANY, i2ws(rme::MapGroundLayer), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, rme::MapMinLayer, rme::MapMaxLayer, rme::MapGroundLayer);
	floor_number->Enable(false);
	floor_options->SetSelection(0);
	tmpsizer->Add(floor_options, 1, wxALL, 5);
	tmpsizer->Add(floor_number, 0, wxALL, 5);
	sizer->Add(tmpsizer, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 5);

	// OK/Cancel buttons
	tmpsizer = newd wxBoxSizer(wxHORIZONTAL);
	tmpsizer->Add(ok_button = newd wxButton(this, wxID_OK, "OK"), wxSizerFlags(1).Center());
	tmpsizer->Add(newd wxButton(this, wxID_CANCEL, "Cancel"), wxSizerFlags(1).Center());
	sizer->Add(tmpsizer, 0, wxCENTER, 10);

	SetSizer(sizer);
	Layout();
	Centre(wxBOTH);
	CheckValues();
}

ExportMiniMapWindow::~ExportMiniMapWindow() = default;

void ExportMiniMapWindow::OnExportTypeChange(wxCommandEvent &event) {
	imageSizeOptions->Enable(event.GetSelection() > 0);
	floor_number->Enable(event.GetSelection() == 2);
}

void ExportMiniMapWindow::OnClickBrowse(wxCommandEvent &WXUNUSED(event)) {
	wxDirDialog dialog(NULL, "Select the output folder", "", wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);
	if (dialog.ShowModal() == wxID_OK) {
		const wxString &directory = dialog.GetPath();
		directory_text_field->ChangeValue(directory);
	}
	CheckValues();
}

void ExportMiniMapWindow::OnDirectoryChanged(wxKeyEvent &event) {
	CheckValues();
	event.Skip();
}

void ExportMiniMapWindow::OnFileNameChanged(wxKeyEvent &event) {
	CheckValues();
	event.Skip();
}

void ExportMiniMapWindow::OnClickOK(wxCommandEvent &WXUNUSED(event)) {
	g_gui.CreateLoadBar("Exporting minimap...");

	const auto imageSizeSelection = imageSizeOptions->GetSelection();
	auto imageSize = 1024;
	if (imageSizeSelection == 1) {
		imageSize = 2048;
	} else if (imageSizeSelection == 2) {
		imageSize = 4096;
	}

	auto format = static_cast<MinimapExportFormat>(format_options->GetSelection());
	auto mode = static_cast<MinimapExportMode>(floor_options->GetSelection());
	std::string directory = directory_text_field->GetValue().ToStdString();
	std::string file_name = file_name_text_field->GetValue().ToStdString();
	int floor = floor_number->GetValue();

	g_settings.setString(Config::MINIMAP_EXPORT_DIR, directory);

	IOMinimap io(&editor, format, mode, true, imageSize);
	if (!io.saveMinimap(directory, file_name, floor)) {
		g_gui.PopupDialog("Error", io.getError(), wxOK);
	}

	g_gui.DestroyLoadBar();
	EndModal(wxID_OK);
}

void ExportMiniMapWindow::OnClickCancel(wxCommandEvent &WXUNUSED(event)) {
	// Just close this window
	EndModal(wxID_CANCEL);
}

void ExportMiniMapWindow::CheckValues() {
	if (directory_text_field->IsEmpty()) {
		error_field->SetLabel("Type or select an output folder.");
		ok_button->Enable(false);
		return;
	}

	if (file_name_text_field->IsEmpty()) {
		error_field->SetLabel("Type a name for the file.");
		ok_button->Enable(false);
		return;
	}

	FileName directory(directory_text_field->GetValue());

	if (!directory.Exists()) {
		error_field->SetLabel("Output folder not found.");
		ok_button->Enable(false);
		return;
	}

	if (!directory.IsDirWritable()) {
		error_field->SetLabel("Output folder is not writable.");
		ok_button->Enable(false);
		return;
	}

	error_field->SetLabel(wxEmptyString);
	ok_button->Enable(true);
}

// ============================================================================
// Export Tilesets window

BEGIN_EVENT_TABLE(ExportTilesetsWindow, wxDialog)
EVT_BUTTON(TILESET_FILE_BUTTON, ExportTilesetsWindow::OnClickBrowse)
EVT_BUTTON(wxID_OK, ExportTilesetsWindow::OnClickOK)
EVT_BUTTON(wxID_CANCEL, ExportTilesetsWindow::OnClickCancel)
END_EVENT_TABLE()

ExportTilesetsWindow::ExportTilesetsWindow(wxWindow* parent, Editor &editor) :
	wxDialog(parent, wxID_ANY, "Export Tilesets", wxDefaultPosition, wxSize(400, 230)),
	editor(editor) {
	wxSizer* sizer = newd wxBoxSizer(wxVERTICAL);
	wxSizer* tmpsizer;

	// Error field
	error_field = newd wxStaticText(this, wxID_VIEW_DETAILS, "", wxDefaultPosition, wxDefaultSize);
	error_field->SetForegroundColour(*wxRED);
	tmpsizer = newd wxBoxSizer(wxHORIZONTAL);
	tmpsizer->Add(error_field, 0, wxALL, 5);
	sizer->Add(tmpsizer, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 5);

	// Output folder
	directory_text_field = newd wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize);
	directory_text_field->Bind(wxEVT_KEY_UP, &ExportTilesetsWindow::OnDirectoryChanged, this);
	directory_text_field->SetValue(wxString(g_settings.getString(Config::TILESET_EXPORT_DIR)));
	tmpsizer = newd wxStaticBoxSizer(wxHORIZONTAL, this, "Output Folder");
	tmpsizer->Add(directory_text_field, 1, wxALL, 5);
	tmpsizer->Add(newd wxButton(this, TILESET_FILE_BUTTON, "Browse"), 0, wxALL, 5);
	sizer->Add(tmpsizer, 0, wxALL | wxEXPAND, 5);

	// File name
	file_name_text_field = newd wxTextCtrl(this, wxID_ANY, "tilesets", wxDefaultPosition, wxDefaultSize);
	file_name_text_field->Bind(wxEVT_KEY_UP, &ExportTilesetsWindow::OnFileNameChanged, this);
	tmpsizer = newd wxStaticBoxSizer(wxHORIZONTAL, this, "File Name");
	tmpsizer->Add(file_name_text_field, 1, wxALL, 5);
	sizer->Add(tmpsizer, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 5);

	// OK/Cancel buttons
	tmpsizer = newd wxBoxSizer(wxHORIZONTAL);
	tmpsizer->Add(ok_button = newd wxButton(this, wxID_OK, "OK"), wxSizerFlags(1).Center());
	tmpsizer->Add(newd wxButton(this, wxID_CANCEL, "Cancel"), wxSizerFlags(1).Center());
	sizer->Add(tmpsizer, 0, wxCENTER, 10);

	SetSizer(sizer);
	Layout();
	Centre(wxBOTH);
	CheckValues();
}

ExportTilesetsWindow::~ExportTilesetsWindow() = default;

void ExportTilesetsWindow::OnClickBrowse(wxCommandEvent &WXUNUSED(event)) {
	wxDirDialog dialog(NULL, "Select the output folder", "", wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);
	if (dialog.ShowModal() == wxID_OK) {
		const wxString &directory = dialog.GetPath();
		directory_text_field->ChangeValue(directory);
	}
	CheckValues();
}

void ExportTilesetsWindow::OnDirectoryChanged(wxKeyEvent &event) {
	CheckValues();
	event.Skip();
}

void ExportTilesetsWindow::OnFileNameChanged(wxKeyEvent &event) {
	CheckValues();
	event.Skip();
}

void ExportTilesetsWindow::OnClickOK(wxCommandEvent &WXUNUSED(event)) {
	g_gui.CreateLoadBar("Exporting Tilesets");

	try {
		FileName directory(directory_text_field->GetValue());
		g_settings.setString(Config::TILESET_EXPORT_DIR, directory_text_field->GetValue().ToStdString());

		FileName file(file_name_text_field->GetValue() + ".xml");
		file.Normalize(wxPATH_NORM_DOTS | wxPATH_NORM_TILDE | wxPATH_NORM_CASE | wxPATH_NORM_ABSOLUTE, directory.GetFullPath());

		pugi::xml_document doc;
		pugi::xml_node node = doc.append_child("materials");

		std::map<std::string, TilesetCategoryType> palettes {
			{ "Terrain", TILESET_TERRAIN },
			{ "Doodad", TILESET_DOODAD },
			{ "Items", TILESET_ITEM },
			{ "Raw", TILESET_RAW }
		};
		for (TilesetContainer::iterator iter = g_materials.tilesets.begin(); iter != g_materials.tilesets.end(); ++iter) {
			std::string _data = iter->second->name;
			std::transform(_data.begin(), _data.end(), _data.begin(), [](unsigned char c) { return std::tolower(c); });
			if (_data == "others") {
				bool blocked = 1;

				for (const auto &kv : palettes) {
					TilesetCategory* tilesetCategory = iter->second->getCategory(kv.second);

					if (kv.second != TILESET_RAW && tilesetCategory->brushlist.size() > 0) {
						blocked = 0;
					}
				}

				if (blocked) {
					continue;
				}
			}

			pugi::xml_node tileset = node.append_child("tileset");
			tileset.append_attribute("name") = iter->second->name.c_str();

			for (const auto &kv : palettes) {
				TilesetCategory* tilesetCategory = iter->second->getCategory(kv.second);

				if (tilesetCategory->brushlist.size() > 0) {
					std::string data = kv.first;
					std::transform(data.begin(), data.end(), data.begin(), [](unsigned char c) { return std::tolower(c); });

					pugi::xml_node palette = tileset.append_child(data.c_str());
					for (BrushVector::const_iterator _iter = tilesetCategory->brushlist.begin(); _iter != tilesetCategory->brushlist.end(); ++_iter) {
						if (!(*_iter)->isRaw()) {
							pugi::xml_node brush = palette.append_child("brush");
							brush.append_attribute("name") = (*_iter)->getName().c_str();
						} else {
							ItemType &it = g_items[(*_iter)->asRaw()->getItemID()];
							if (it.id != 0) {
								pugi::xml_node item = palette.append_child("item");
								item.append_attribute("id") = it.id;
							}
						}
					}
				}
			}

			size_t n = std::distance(tileset.begin(), tileset.end());
			if (n <= 0) {
				node.remove_child(tileset);
			}
		}

		doc.save_file(file.GetFullPath().mb_str());
		g_gui.PopupDialog("Successfully saved Tilesets", "Saved tilesets to '" + std::string(file.GetFullPath().mb_str()) + "'", wxOK);
		g_materials.modify(false);
	} catch (std::bad_alloc &) {
		g_gui.PopupDialog("Error", "There is not enough memory available to complete the operation.", wxOK);
	}

	g_gui.DestroyLoadBar();
	EndModal(1);
}

void ExportTilesetsWindow::OnClickCancel(wxCommandEvent &WXUNUSED(event)) {
	// Just close this window
	EndModal(0);
}

void ExportTilesetsWindow::CheckValues() {
	if (directory_text_field->IsEmpty()) {
		error_field->SetLabel("Type or select an output folder.");
		ok_button->Enable(false);
		return;
	}

	if (file_name_text_field->IsEmpty()) {
		error_field->SetLabel("Type a name for the file.");
		ok_button->Enable(false);
		return;
	}

	FileName directory(directory_text_field->GetValue());

	if (!directory.Exists()) {
		error_field->SetLabel("Output folder not found.");
		ok_button->Enable(false);
		return;
	}

	if (!directory.IsDirWritable()) {
		error_field->SetLabel("Output folder is not writable.");
		ok_button->Enable(false);
		return;
	}

	error_field->SetLabel(wxEmptyString);
	ok_button->Enable(true);
}

// ============================================================================
// Numkey forwarding text control

BEGIN_EVENT_TABLE(KeyForwardingTextCtrl, wxTextCtrl)
EVT_KEY_DOWN(KeyForwardingTextCtrl::OnKeyDown)
END_EVENT_TABLE()

void KeyForwardingTextCtrl::OnKeyDown(wxKeyEvent &event) {
	if (event.GetKeyCode() == WXK_UP || event.GetKeyCode() == WXK_DOWN || event.GetKeyCode() == WXK_PAGEDOWN || event.GetKeyCode() == WXK_PAGEUP) {
		GetParent()->GetEventHandler()->AddPendingEvent(event);
	} else {
		event.Skip();
	}
}

// ============================================================================
// Find Item Dialog (Jump to item)

BEGIN_EVENT_TABLE(FindDialog, wxDialog)
EVT_TIMER(wxID_ANY, FindDialog::OnTextIdle)
EVT_TEXT(JUMP_DIALOG_TEXT, FindDialog::OnTextChange)
EVT_KEY_DOWN(FindDialog::OnKeyDown)
EVT_TEXT_ENTER(JUMP_DIALOG_TEXT, FindDialog::OnClickOK)
EVT_LISTBOX_DCLICK(JUMP_DIALOG_LIST, FindDialog::OnClickList)
EVT_BUTTON(wxID_OK, FindDialog::OnClickOK)
EVT_BUTTON(wxID_CANCEL, FindDialog::OnClickCancel)
END_EVENT_TABLE()

FindDialog::FindDialog(wxWindow* parent, wxString title) :
	wxDialog(g_gui.root, wxID_ANY, title, wxDefaultPosition, wxDefaultSize, wxRESIZE_BORDER | wxCAPTION | wxCLOSE_BOX),
	idle_input_timer(this),
	result_brush(nullptr),
	result_id(0) {
	wxSizer* sizer = newd wxBoxSizer(wxVERTICAL);

	search_field = newd KeyForwardingTextCtrl(this, JUMP_DIALOG_TEXT, "", wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
	search_field->SetFocus();
	sizer->Add(search_field, 0, wxEXPAND);

	item_list = newd FindDialogListBox(this, JUMP_DIALOG_LIST);
	item_list->SetMinSize(wxSize(470, 400));
	sizer->Add(item_list, wxSizerFlags(1).Expand().Border());

	wxSizer* stdsizer = newd wxBoxSizer(wxHORIZONTAL);
	stdsizer->Add(newd wxButton(this, wxID_OK, "OK"), wxSizerFlags(1).Center());
	stdsizer->Add(newd wxButton(this, wxID_CANCEL, "Cancel"), wxSizerFlags(1).Center());
	sizer->Add(stdsizer, wxSizerFlags(0).Center().Border());

	SetSizerAndFit(sizer);
	Centre(wxBOTH);
	// We can't call it here since it calls an abstract function, call in child constructors instead.
	// RefreshContents();
}

FindDialog::~FindDialog() = default;

void FindDialog::OnKeyDown(wxKeyEvent &event) {
	int w, h;
	item_list->GetSize(&w, &h);
	size_t amount = 1;

	switch (event.GetKeyCode()) {
		case WXK_PAGEUP:
			amount = h / 32 + 1;
			[[fallthrough]];
		case WXK_UP: {
			if (item_list->GetItemCount() > 0) {
				ssize_t n = item_list->GetSelection();
				if (n == wxNOT_FOUND) {
					n = 0;
				} else if (n != amount && n - amount < n) { // latter is needed for unsigned overflow
					n -= amount;
				} else {
					n = 0;
				}
				item_list->SetSelection(n);
			}
			break;
		}

		case WXK_PAGEDOWN:
			amount = h / 32 + 1;
			[[fallthrough]];
		case WXK_DOWN: {
			if (item_list->GetItemCount() > 0) {
				ssize_t n = item_list->GetSelection();
				size_t itemcount = item_list->GetItemCount();
				if (n == wxNOT_FOUND) {
					n = 0;
				} else if (static_cast<uint32_t>(n) < itemcount - amount && itemcount - amount < itemcount) {
					n += amount;
				} else {
					n = item_list->GetItemCount() - 1;
				}

				item_list->SetSelection(n);
			}
			break;
		}
		default:
			event.Skip();
			break;
	}
}

void FindDialog::OnTextIdle(wxTimerEvent &WXUNUSED(event)) {
	RefreshContents();
}

void FindDialog::OnTextChange(wxCommandEvent &WXUNUSED(event)) {
	idle_input_timer.Start(800, true);
}

void FindDialog::OnClickList(wxCommandEvent &event) {
	OnClickListInternal(event);
}

void FindDialog::OnClickOK(wxCommandEvent &WXUNUSED(event)) {
	// This is to get virtual callback
	OnClickOKInternal();
}

void FindDialog::OnClickCancel(wxCommandEvent &WXUNUSED(event)) {
	EndModal(0);
}

void FindDialog::RefreshContents() {
	// This is to get virtual callback
	RefreshContentsInternal();
}

// ============================================================================
// Find Brush Dialog (Jump to brush)

FindBrushDialog::FindBrushDialog(wxWindow* parent, wxString title) :
	FindDialog(parent, title) {
	RefreshContents();
}

FindBrushDialog::~FindBrushDialog() = default;

void FindBrushDialog::OnClickListInternal(wxCommandEvent &event) {
	Brush* brush = item_list->GetSelectedBrush();
	if (brush) {
		result_brush = brush;
		EndModal(1);
	}
}

void FindBrushDialog::OnClickOKInternal() {
	// This is kind of stupid as it would fail unless the "Please enter a search string" wasn't there
	if (item_list->GetItemCount() > 0) {
		if (item_list->GetSelection() == wxNOT_FOUND) {
			item_list->SetSelection(0);
		}
		Brush* brush = item_list->GetSelectedBrush();
		if (!brush) {
			// It's either "Please enter a search string" or "No matches"
			// Perhaps we can refresh now?
			std::string search_string = as_lower_str(nstr(search_field->GetValue()));
			bool do_search = (search_string.size() >= 2);

			if (do_search) {
				const BrushMap &map = g_brushes.getMap();
				for (BrushMap::const_iterator iter = map.begin(); iter != map.end(); ++iter) {
					const Brush* brush = iter->second;
					if (as_lower_str(brush->getName()).find(search_string) == std::string::npos) {
						continue;
					}

					// Don't match RAWs now.
					if (brush->isRaw()) {
						continue;
					}

					// Found one!
					result_brush = brush;
					break;
				}

				// Did we not find a matching brush?
				if (!result_brush) {
					// Then let's search the RAWs
					for (int id = 0; id <= g_items.getMaxID(); ++id) {
						const ItemType &type = g_items.getItemType(id);
						if (type.id == 0) {
							continue;
						}

						RAWBrush* raw_brush = type.raw_brush;
						if (!raw_brush) {
							continue;
						}

						if (as_lower_str(raw_brush->getName()).find(search_string) == std::string::npos) {
							continue;
						}

						// Found one!
						result_brush = raw_brush;
						break;
					}
				}
				// Done!
			}
		} else {
			result_brush = brush;
		}
	}
	EndModal(1);
}

void FindBrushDialog::RefreshContentsInternal() {
	item_list->Clear();

	std::string search_string = as_lower_str(nstr(search_field->GetValue()));
	bool do_search = (search_string.size() >= 2);

	if (do_search) {

		bool found_search_results = false;

		const BrushMap &brushes_map = g_brushes.getMap();

		// We store the raws so they display last of all results
		std::deque<const RAWBrush*> raws;

		for (BrushMap::const_iterator iter = brushes_map.begin(); iter != brushes_map.end(); ++iter) {
			const Brush* brush = iter->second;

			if (as_lower_str(brush->getName()).find(search_string) == std::string::npos) {
				continue;
			}

			if (brush->isRaw()) {
				continue;
			}

			found_search_results = true;
			item_list->AddBrush(const_cast<Brush*>(brush));
		}

		for (int id = 0; id <= g_items.getMaxID(); ++id) {
			const ItemType &type = g_items.getItemType(id);
			if (type.id == 0) {
				continue;
			}

			RAWBrush* raw_brush = type.raw_brush;
			if (!raw_brush) {
				continue;
			}

			if (as_lower_str(raw_brush->getName()).find(search_string) == std::string::npos) {
				continue;
			}

			found_search_results = true;
			item_list->AddBrush(raw_brush);
		}

		while (raws.size() > 0) {
			item_list->AddBrush(const_cast<RAWBrush*>(raws.front()));
			raws.pop_front();
		}

		if (found_search_results) {
			item_list->SetSelection(0);
		} else {
			item_list->SetNoMatches();
		}
	}
	item_list->Refresh();
}

// ============================================================================
// Listbox in find item / brush stuff

FindDialogListBox::FindDialogListBox(wxWindow* parent, wxWindowID id) :
	wxVListBox(parent, id, wxDefaultPosition, wxDefaultSize, wxLB_SINGLE),
	cleared(false),
	no_matches(false) {
	Clear();
}

FindDialogListBox::~FindDialogListBox() {
	////
}

void FindDialogListBox::Clear() {
	cleared = true;
	no_matches = false;
	brushlist.clear();
	SetItemCount(1);
}

void FindDialogListBox::SetNoMatches() {
	cleared = false;
	no_matches = true;
	brushlist.clear();
	SetItemCount(1);
}

void FindDialogListBox::AddBrush(Brush* brush) {
	if (cleared || no_matches) {
		SetItemCount(0);
	}

	cleared = false;
	no_matches = false;

	SetItemCount(GetItemCount() + 1);
	brushlist.push_back(brush);
}

Brush* FindDialogListBox::GetSelectedBrush() {
	ssize_t n = GetSelection();
	if (n == wxNOT_FOUND || no_matches || cleared) {
		return nullptr;
	}
	return brushlist[n];
}

void FindDialogListBox::OnDrawItem(wxDC &dc, const wxRect &rect, size_t n) const {
	if (no_matches) {
		dc.DrawText("No matches for your search.", rect.GetX() + 40, rect.GetY() + 6);
	} else if (cleared) {
		dc.DrawText("Please enter your search string.", rect.GetX() + 40, rect.GetY() + 6);
	} else {
		ASSERT(n < brushlist.size());
		Sprite* spr = g_gui.gfx.getSprite(brushlist[n]->getLookID());
		if (spr) {
			spr->DrawTo(&dc, SPRITE_SIZE_32x32, rect.GetX(), rect.GetY(), rect.GetWidth(), rect.GetHeight());
		} else {
			auto monsterType = g_monsters[brushlist[n]->getName()];
			NpcType* npcType;
			if (!monsterType) {
				npcType = g_npcs[brushlist[n]->getName()];
			}

			auto lookType = monsterType ? monsterType->outfit.lookType : npcType ? npcType->outfit.lookType
																				 : 0;
			auto creatureSprite = g_gui.gfx.getCreatureSprite(lookType);
			if (creatureSprite) {
				creatureSprite->DrawTo(&dc, SPRITE_SIZE_32x32, rect.GetX(), rect.GetY(), rect.GetWidth(), rect.GetHeight());
			}
		}

		if (IsSelected(n)) {
			if (HasFocus()) {
				dc.SetTextForeground(wxColor(0xFF, 0xFF, 0xFF));
			} else {
				dc.SetTextForeground(wxColor(0x00, 0x00, 0xFF));
			}
		} else {
			dc.SetTextForeground(wxColor(0x00, 0x00, 0x00));
		}

		dc.DrawText(wxstr(brushlist[n]->getName()), rect.GetX() + 40, rect.GetY() + 6);
	}
}

wxCoord FindDialogListBox::OnMeasureItem(size_t n) const {
	return 32;
}

// ============================================================================
// wxListBox that can be sorted

SortableListBox::SortableListBox(wxWindow* parent, wxWindowID id, const wxPoint &pos, const wxSize &size) :
	wxCheckListBox(parent, id, pos, size, 0, nullptr, wxLB_SINGLE | wxLB_NEEDED_SB) { }

SortableListBox::~SortableListBox() { }

void SortableListBox::Sort() {
	if (GetCount() == 0) {
		return;
	}

	wxASSERT_MSG(GetClientDataType() != wxClientData_Object, "Sorting a list with data of type wxClientData_Object is currently not implemented");

	DoSort();
}

void SortableListBox::DoSort() {
	size_t count = GetCount();
	int selection = GetSelection();
	wxClientDataType dataType = GetClientDataType();

	wxArrayString stringList;
	wxArrayPtrVoid dataList;

	for (size_t i = 0; i < count; ++i) {
		stringList.Add(GetString(i));
		if (dataType == wxClientData_Void) {
			dataList.Add(GetClientData(i));
		}
	}

	// Insertion sort
	for (size_t i = 0; i < count; ++i) {
		size_t j = i;
		while (j > 0 && stringList[j].CmpNoCase(stringList[j - 1]) < 0) {

			wxString tmpString = stringList[j];
			stringList[j] = stringList[j - 1];
			stringList[j - 1] = tmpString;

			if (dataType == wxClientData_Void) {
				void* tmpData = dataList[j];
				dataList[j] = dataList[j - 1];
				dataList[j - 1] = tmpData;
			}

			if (selection == j - 1) {
				selection++;
			} else if (selection == j) {
				selection--;
			}

			j--;
		}
	}

	Freeze();
	Clear();
	for (size_t i = 0; i < count; ++i) {
		if (dataType == wxClientData_Void) {
			Append(stringList[i], dataList[i]);
		} else {
			Append(stringList[i]);
		}
	}
	Thaw();

	SetSelection(selection);
}

// ============================================================================
// Object properties base

ObjectPropertiesWindowBase::ObjectPropertiesWindowBase(wxWindow* parent, wxString title, const Map* map, const Tile* tile, Item* item, wxPoint position /* = wxDefaultPosition */) :
	wxDialog(parent, wxID_ANY, title, position, wxSize(600, 400), wxCAPTION | wxCLOSE_BOX | wxRESIZE_BORDER),
	edit_map(map),
	edit_tile(tile),
	edit_item(item),
	edit_monster(nullptr),
	edit_spawn_monster(nullptr),
	edit_npc(nullptr),
	edit_spawn_npc(nullptr) {
	////
}

ObjectPropertiesWindowBase::ObjectPropertiesWindowBase(wxWindow* parent, wxString title, const Map* map, const Tile* tile, Monster* monster, wxPoint position /* = wxDefaultPosition */) :
	wxDialog(parent, wxID_ANY, title, position, wxSize(600, 400), wxCAPTION | wxCLOSE_BOX | wxRESIZE_BORDER),
	edit_map(map),
	edit_tile(tile),
	edit_item(nullptr),
	edit_monster(monster),
	edit_spawn_monster(nullptr),
	edit_npc(nullptr),
	edit_spawn_npc(nullptr) {
	////
}

ObjectPropertiesWindowBase::ObjectPropertiesWindowBase(wxWindow* parent, wxString title, const Map* map, const Tile* tile, SpawnMonster* spawnMonster, wxPoint position /* = wxDefaultPosition */) :
	wxDialog(parent, wxID_ANY, title, position, wxSize(600, 400), wxCAPTION | wxCLOSE_BOX | wxRESIZE_BORDER),
	edit_map(map),
	edit_tile(tile),
	edit_item(nullptr),
	edit_monster(nullptr),
	edit_spawn_monster(spawnMonster),
	edit_npc(nullptr),
	edit_spawn_npc(nullptr) {
	////
}

ObjectPropertiesWindowBase::ObjectPropertiesWindowBase(wxWindow* parent, wxString title, const Map* map, const Tile* tile, Npc* npc, wxPoint position /* = wxDefaultPosition */) :
	wxDialog(parent, wxID_ANY, title, position, wxSize(600, 400), wxCAPTION | wxCLOSE_BOX | wxRESIZE_BORDER),
	edit_map(map),
	edit_tile(tile),
	edit_item(nullptr),
	edit_monster(nullptr),
	edit_spawn_monster(nullptr),
	edit_npc(npc),
	edit_spawn_npc(nullptr) {
	////
}

ObjectPropertiesWindowBase::ObjectPropertiesWindowBase(wxWindow* parent, wxString title, const Map* map, const Tile* tile, SpawnNpc* spawnNpc, wxPoint position /* = wxDefaultPosition */) :
	wxDialog(parent, wxID_ANY, title, position, wxSize(600, 400), wxCAPTION | wxCLOSE_BOX | wxRESIZE_BORDER),
	edit_map(map),
	edit_tile(tile),
	edit_item(nullptr),
	edit_monster(nullptr),
	edit_spawn_monster(nullptr),
	edit_npc(nullptr),
	edit_spawn_npc(spawnNpc) {
	////
}

ObjectPropertiesWindowBase::ObjectPropertiesWindowBase(wxWindow* parent, wxString title, wxPoint position /* = wxDefaultPosition */) :
	wxDialog(parent, wxID_ANY, title, position, wxSize(600, 400), wxCAPTION | wxCLOSE_BOX | wxRESIZE_BORDER) {
	////
}

Item* ObjectPropertiesWindowBase::getItemBeingEdited() {
	return edit_item;
}

// ============================================================================
// Edit Towns Dialog

BEGIN_EVENT_TABLE(EditTownsDialog, wxDialog)
EVT_LISTBOX(EDIT_TOWNS_LISTBOX, EditTownsDialog::OnListBoxChange)

EVT_BUTTON(EDIT_TOWNS_SELECT_TEMPLE, EditTownsDialog::OnClickSelectTemplePosition)
EVT_BUTTON(EDIT_TOWNS_ADD, EditTownsDialog::OnClickAdd)
EVT_BUTTON(EDIT_TOWNS_REMOVE, EditTownsDialog::OnClickRemove)
EVT_BUTTON(wxID_OK, EditTownsDialog::OnClickOK)
EVT_BUTTON(wxID_CANCEL, EditTownsDialog::OnClickCancel)
END_EVENT_TABLE()

EditTownsDialog::EditTownsDialog(wxWindow* parent, Editor &editor) :
	wxDialog(parent, wxID_ANY, "Towns", wxDefaultPosition, wxSize(280, 330)),
	editor(editor) {
	const Map &map = editor.getMap();

	// Create topsizer
	wxSizer* sizer = newd wxBoxSizer(wxVERTICAL);
	wxSizer* tmpsizer;

	for (TownMap::const_iterator town_iter = map.towns.begin(); town_iter != map.towns.end(); ++town_iter) {
		Town* town = town_iter->second;
		town_list.push_back(newd Town(*town));
		if (max_town_id < town->getID()) {
			max_town_id = town->getID();
		}
	}

	// Town list
	town_listbox = newd wxListBox(this, EDIT_TOWNS_LISTBOX, wxDefaultPosition, wxSize(240, 100));
	sizer->Add(town_listbox, 1, wxEXPAND | wxTOP | wxLEFT | wxRIGHT, 10);

	tmpsizer = newd wxBoxSizer(wxHORIZONTAL);
	tmpsizer->Add(newd wxButton(this, EDIT_TOWNS_ADD, "Add"), 0, wxTOP, 5);
	tmpsizer->Add(remove_button = newd wxButton(this, EDIT_TOWNS_REMOVE, "Remove"), 0, wxRIGHT | wxTOP, 5);
	sizer->Add(tmpsizer, 0, wxEXPAND | wxLEFT | wxRIGHT, 10);

	// House options
	tmpsizer = newd wxStaticBoxSizer(wxHORIZONTAL, this, "Name / ID");
	name_field = newd wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxSize(190, 20), 0, wxTextValidator(wxFILTER_ASCII, &town_name));
	tmpsizer->Add(name_field, 2, wxEXPAND | wxLEFT | wxBOTTOM, 5);

	id_field = newd wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxSize(40, 20), 0, wxTextValidator(wxFILTER_NUMERIC, &town_id));
	id_field->Enable(false);
	tmpsizer->Add(id_field, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
	sizer->Add(tmpsizer, 0, wxEXPAND | wxALL, 10);

	// Temple position
	temple_position = newd PositionCtrl(this, "Temple Position", 0, 0, 0, map.getWidth(), map.getHeight());
	select_position_button = newd wxButton(this, EDIT_TOWNS_SELECT_TEMPLE, "Go To");
	temple_position->Add(select_position_button, 0, wxLEFT | wxRIGHT | wxBOTTOM, 5);
	sizer->Add(temple_position, 0, wxEXPAND | wxLEFT | wxRIGHT, 10);

	// OK/Cancel buttons
	tmpsizer = newd wxBoxSizer(wxHORIZONTAL);
	tmpsizer->Add(newd wxButton(this, wxID_OK, "OK"), wxSizerFlags(1).Center());
	tmpsizer->Add(newd wxButton(this, wxID_CANCEL, "Cancel"), wxSizerFlags(1).Center());
	sizer->Add(tmpsizer, 0, wxCENTER | wxALL, 10);

	SetSizerAndFit(sizer);
	Centre(wxBOTH);
	BuildListBox(true);
}

EditTownsDialog::~EditTownsDialog() {
	for (std::vector<Town*>::iterator town_iter = town_list.begin(); town_iter != town_list.end(); ++town_iter) {
		delete *town_iter;
	}
}

void EditTownsDialog::BuildListBox(bool doselect) {
	long tmplong = 0;
	max_town_id = 0;
	wxArrayString town_name_list;
	uint32_t selection_before = 0;

	if (doselect && id_field->GetValue().ToLong(&tmplong)) {
		uint32_t old_town_id = tmplong;

		for (std::vector<Town*>::iterator town_iter = town_list.begin(); town_iter != town_list.end(); ++town_iter) {
			if (old_town_id == (*town_iter)->getID()) {
				selection_before = (*town_iter)->getID();
				break;
			}
		}
	}

	for (std::vector<Town*>::iterator town_iter = town_list.begin(); town_iter != town_list.end(); ++town_iter) {
		Town* town = *town_iter;
		town_name_list.Add(wxstr(town->getName()));
		if (max_town_id < town->getID()) {
			max_town_id = town->getID();
		}
	}

	town_listbox->Set(town_name_list);
	remove_button->Enable(town_listbox->GetCount() != 0);
	select_position_button->Enable(false);

	if (doselect) {
		if (selection_before) {
			int i = 0;
			for (std::vector<Town*>::iterator town_iter = town_list.begin(); town_iter != town_list.end(); ++town_iter) {
				if (selection_before == (*town_iter)->getID()) {
					town_listbox->SetSelection(i);
					return;
				}
				++i;
			}
		}
		UpdateSelection(0);
	}
}

void EditTownsDialog::UpdateSelection(int new_selection) {
	long tmplong;

	// Save old values
	if (town_list.size() > 0) {
		if (id_field->GetValue().ToLong(&tmplong)) {
			uint32_t old_town_id = tmplong;

			Town* old_town = nullptr;

			for (std::vector<Town*>::iterator town_iter = town_list.begin(); town_iter != town_list.end(); ++town_iter) {
				if (old_town_id == (*town_iter)->getID()) {
					old_town = *town_iter;
					break;
				}
			}

			if (old_town) {
				Position templepos = temple_position->GetPosition();

				// printf("Changed town %d:%s\n", old_town_id, old_town->getName().c_str());
				// printf("New values %d:%s:%d:%d:%d\n", town_id, town_name.c_str(), templepos.x, templepos.y, templepos.z);
				old_town->setTemplePosition(templepos);

				wxString new_name = name_field->GetValue();
				wxString old_name = wxstr(old_town->getName());

				old_town->setName(nstr(new_name));
				if (new_name != old_name) {
					// Name has changed, update list
					BuildListBox(false);
				}
			}
		}
	}

	// Clear fields
	town_name.Clear();
	town_id.Clear();

	if (town_list.size() > size_t(new_selection)) {
		name_field->Enable(true);
		temple_position->Enable(true);
		select_position_button->Enable(true);

		// Change the values to reflect the newd selection
		Town* town = town_list[new_selection];
		ASSERT(town);

		// printf("Selected %d:%s\n", new_selection, town->getName().c_str());
		town_name << wxstr(town->getName());
		name_field->SetValue(town_name);
		town_id << long(town->getID());
		id_field->SetValue(town_id);
		temple_position->SetPosition(town->getTemplePosition());
		town_listbox->SetSelection(new_selection);
	} else {
		name_field->Enable(false);
		temple_position->Enable(false);
		select_position_button->Enable(false);
	}
	Refresh();
}

void EditTownsDialog::OnListBoxChange(wxCommandEvent &event) {
	UpdateSelection(event.GetSelection());
}

void EditTownsDialog::OnClickSelectTemplePosition(wxCommandEvent &WXUNUSED(event)) {
	Position templepos = temple_position->GetPosition();
	g_gui.SetScreenCenterPosition(templepos);
}

void EditTownsDialog::OnClickAdd(wxCommandEvent &WXUNUSED(event)) {
	Town* new_town = newd Town(++max_town_id);
	new_town->setName("Unnamed Town");
	new_town->setTemplePosition(Position(0, 0, 0));
	town_list.push_back(new_town);

	BuildListBox(false);
	UpdateSelection(town_list.size() - 1);
	town_listbox->SetSelection(town_list.size() - 1);
}

void EditTownsDialog::OnClickRemove(wxCommandEvent &WXUNUSED(event)) {
	long tmplong;
	if (id_field->GetValue().ToLong(&tmplong)) {
		uint32_t old_town_id = tmplong;

		Town* town = nullptr;

		std::vector<Town*>::iterator town_iter = town_list.begin();

		int selection_index = 0;
		while (town_iter != town_list.end()) {
			if (old_town_id == (*town_iter)->getID()) {
				town = *town_iter;
				break;
			}
			++selection_index;
			++town_iter;
		}
		if (!town) {
			return;
		}

		const Map &map = editor.getMap();
		for (const auto &pair : map.houses) {
			if (pair.second->townid == town->getID()) {
				g_gui.PopupDialog(this, "Error", "You cannot delete a town which still has houses associated with it.", wxOK);
				return;
			}
		}

		delete town;
		town_list.erase(town_iter);
		BuildListBox(false);
		UpdateSelection(selection_index - 1);
	}
}

void EditTownsDialog::OnClickOK(wxCommandEvent &WXUNUSED(event)) {
	long tmplong = 0;

	if (Validate() && TransferDataFromWindow()) {
		// Save old values
		if (town_list.size() > 0 && id_field->GetValue().ToLong(&tmplong)) {
			uint32_t old_town_id = tmplong;

			Town* old_town = nullptr;

			for (std::vector<Town*>::iterator town_iter = town_list.begin(); town_iter != town_list.end(); ++town_iter) {
				if (old_town_id == (*town_iter)->getID()) {
					old_town = *town_iter;
					break;
				}
			}

			if (old_town) {
				Position templepos = temple_position->GetPosition();

				// printf("Changed town %d:%s\n", old_town_id, old_town->getName().c_str());
				// printf("New values %d:%s:%d:%d:%d\n", town_id, town_name.c_str(), templepos.x, templepos.y, templepos.z);
				old_town->setTemplePosition(templepos);

				wxString new_name = name_field->GetValue();
				wxString old_name = wxstr(old_town->getName());

				old_town->setName(nstr(new_name));
				if (new_name != old_name) {
					// Name has changed, update list
					BuildListBox(true);
				}
			}
		}

		Towns &towns = editor.getMap().towns;

		// Verify the newd information
		for (std::vector<Town*>::iterator town_iter = town_list.begin(); town_iter != town_list.end(); ++town_iter) {
			Town* town = *town_iter;
			if (town->getName() == "") {
				g_gui.PopupDialog(this, "Error", "You can't have a town with an empty name.", wxOK);
				return;
			}
			if (!town->getTemplePosition().isValid() || town->getTemplePosition().x > editor.getMap().getWidth() || town->getTemplePosition().y > editor.getMap().getHeight()) {
				wxString msg;
				msg << "The town " << wxstr(town->getName()) << " has an invalid temple position.";
				g_gui.PopupDialog(this, "Error", msg, wxOK);
				return;
			}
		}

		// Clear old towns
		towns.clear();

		// Build the newd town map
		for (std::vector<Town*>::iterator town_iter = town_list.begin(); town_iter != town_list.end(); ++town_iter) {
			towns.addTown(*town_iter);
		}
		town_list.clear();
		editor.getMap().doChange();

		EndModal(1);
		g_gui.RefreshPalettes();
	}
}

void EditTownsDialog::OnClickCancel(wxCommandEvent &WXUNUSED(event)) {
	// Just close this window
	EndModal(0);
}

// ============================================================================
// Go To Position Dialog
// Jump to a position on the map by entering XYZ coordinates

BEGIN_EVENT_TABLE(GotoPositionDialog, wxDialog)
EVT_BUTTON(wxID_OK, GotoPositionDialog::OnClickOK)
EVT_BUTTON(wxID_CANCEL, GotoPositionDialog::OnClickCancel)
END_EVENT_TABLE()

GotoPositionDialog::GotoPositionDialog(wxWindow* parent, Editor &editor) :
	wxDialog(parent, wxID_ANY, "Go To Position", wxDefaultPosition, wxDefaultSize),
	editor(editor) {
	const Map &map = editor.getMap();

	// create topsizer
	wxSizer* sizer = newd wxBoxSizer(wxVERTICAL);

	posctrl = newd PositionCtrl(this, "Destination", map.getWidth() / 2, map.getHeight() / 2, rme::MapGroundLayer, map.getWidth(), map.getHeight());
	sizer->Add(posctrl, 0, wxTOP | wxLEFT | wxRIGHT, 20);

	// OK/Cancel buttons
	wxSizer* tmpsizer = newd wxBoxSizer(wxHORIZONTAL);
	tmpsizer->Add(newd wxButton(this, wxID_OK, "OK"), wxSizerFlags(1).Center());
	tmpsizer->Add(newd wxButton(this, wxID_CANCEL, "Cancel"), wxSizerFlags(1).Center());
	sizer->Add(tmpsizer, 0, wxALL | wxCENTER, 20); // Border to top too

	SetSizerAndFit(sizer);
	Centre(wxBOTH);
}

void GotoPositionDialog::OnClickCancel(wxCommandEvent &) {
	EndModal(0);
}

void GotoPositionDialog::OnClickOK(wxCommandEvent &) {
	g_gui.SetScreenCenterPosition(posctrl->GetPosition());
	EndModal(1);
}
