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

#include "bitmap_to_map_window.h"
#include "bitmap_to_map_converter.h"
#include "editor.h"
#include "brush.h"
#include "ground_brush.h"
#include "items.h"
#include "gui.h"
#include "common.h"

BEGIN_EVENT_TABLE(BitmapToMapWindow, wxDialog)
EVT_BUTTON(BITMAP_TO_MAP_BROWSE, BitmapToMapWindow::OnClickBrowse)
EVT_BUTTON(BITMAP_TO_MAP_GENERATE, BitmapToMapWindow::OnClickGenerate)
EVT_BUTTON(BITMAP_TO_MAP_PREVIEW, BitmapToMapWindow::OnClickPreview)
EVT_BUTTON(BITMAP_TO_MAP_ROTATE_LEFT, BitmapToMapWindow::OnClickRotateLeft)
EVT_BUTTON(BITMAP_TO_MAP_ROTATE_RIGHT, BitmapToMapWindow::OnClickRotateRight)
EVT_BUTTON(BITMAP_TO_MAP_FLIP, BitmapToMapWindow::OnClickFlip)
EVT_BUTTON(BITMAP_TO_MAP_CROP, BitmapToMapWindow::OnClickCrop)
EVT_BUTTON(BITMAP_TO_MAP_SAVE_PRESET, BitmapToMapWindow::OnClickSavePreset)
EVT_BUTTON(BITMAP_TO_MAP_LOAD_PRESET, BitmapToMapWindow::OnClickLoadPreset)
EVT_BUTTON(BITMAP_TO_MAP_DELETE_COLOR, BitmapToMapWindow::OnClickDeleteColor)
EVT_TEXT(BITMAP_TO_MAP_FILTER, BitmapToMapWindow::OnFilterColors)
EVT_LIST_ITEM_ACTIVATED(BITMAP_TO_MAP_COLOR_LIST, BitmapToMapWindow::OnColorListActivated)
END_EVENT_TABLE()

BitmapToMapWindow::BitmapToMapWindow(wxWindow* parent, Editor &editor) :
	wxDialog(parent, wxID_ANY, "Bitmap to Map", wxDefaultPosition, wxSize(900, 650)),
	editor(editor),
	imageLoaded(false),
	imagePreview(nullptr),
	previewPanel(nullptr),
	colorListCtrl(nullptr),
	filterCtrl(nullptr),
	toleranceCtrl(nullptr),
	xOffsetCtrl(nullptr),
	yOffsetCtrl(nullptr),
	zOffsetCtrl(nullptr),
	imageInfoLabel(nullptr),
	progressBar(nullptr) {
	wxBoxSizer* mainSizer = newd wxBoxSizer(wxHORIZONTAL);

	// === Left panel: image preview ===
	wxBoxSizer* leftSizer = newd wxBoxSizer(wxVERTICAL);

	imageInfoLabel = newd wxStaticText(this, wxID_ANY, "No image loaded");
	leftSizer->Add(imageInfoLabel, 0, wxALL, 5);

	previewPanel = newd wxScrolledWindow(this, wxID_ANY, wxDefaultPosition, wxSize(400, 400));
	previewPanel->SetScrollRate(5, 5);
	previewPanel->SetBackgroundColour(*wxBLACK);
	imagePreview = newd wxStaticBitmap(previewPanel, wxID_ANY, wxNullBitmap);
	leftSizer->Add(previewPanel, 1, wxEXPAND | wxALL, 5);

	// Image manipulation buttons
	wxBoxSizer* imgBtnSizer = newd wxBoxSizer(wxHORIZONTAL);
	imgBtnSizer->Add(newd wxButton(this, BITMAP_TO_MAP_ROTATE_LEFT, "Rotate L"), 0, wxALL, 2);
	imgBtnSizer->Add(newd wxButton(this, BITMAP_TO_MAP_ROTATE_RIGHT, "Rotate R"), 0, wxALL, 2);
	imgBtnSizer->Add(newd wxButton(this, BITMAP_TO_MAP_FLIP, "Flip H"), 0, wxALL, 2);
	imgBtnSizer->Add(newd wxButton(this, BITMAP_TO_MAP_CROP, "Crop"), 0, wxALL, 2);
	leftSizer->Add(imgBtnSizer, 0, wxALIGN_CENTER);

	mainSizer->Add(leftSizer, 1, wxEXPAND);

	// === Right panel: controls + color list ===
	wxBoxSizer* rightSizer = newd wxBoxSizer(wxVERTICAL);

	// Browse
	wxBoxSizer* browseSizer = newd wxBoxSizer(wxHORIZONTAL);
	browseSizer->Add(newd wxButton(this, BITMAP_TO_MAP_BROWSE, "Browse Image..."), 0, wxALL, 5);
	rightSizer->Add(browseSizer, 0, wxEXPAND);

	// Offsets
	wxStaticBoxSizer* offsetBox = newd wxStaticBoxSizer(wxHORIZONTAL, this, "Offsets");
	offsetBox->Add(newd wxStaticText(offsetBox->GetStaticBox(), wxID_ANY, "X:"), 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 5);
	xOffsetCtrl = newd wxSpinCtrl(offsetBox->GetStaticBox(), wxID_ANY, "0", wxDefaultPosition, wxSize(70, -1), wxSP_ARROW_KEYS, 0, 65000, 0);
	offsetBox->Add(xOffsetCtrl, 0, wxALL, 2);
	offsetBox->Add(newd wxStaticText(offsetBox->GetStaticBox(), wxID_ANY, "Y:"), 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 5);
	yOffsetCtrl = newd wxSpinCtrl(offsetBox->GetStaticBox(), wxID_ANY, "0", wxDefaultPosition, wxSize(70, -1), wxSP_ARROW_KEYS, 0, 65000, 0);
	offsetBox->Add(yOffsetCtrl, 0, wxALL, 2);
	offsetBox->Add(newd wxStaticText(offsetBox->GetStaticBox(), wxID_ANY, "Z:"), 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 5);
	zOffsetCtrl = newd wxSpinCtrl(offsetBox->GetStaticBox(), wxID_ANY, "7", wxDefaultPosition, wxSize(50, -1), wxSP_ARROW_KEYS, 0, 15, 7);
	offsetBox->Add(zOffsetCtrl, 0, wxALL, 2);
	rightSizer->Add(offsetBox, 0, wxEXPAND | wxALL, 5);

	// Tolerance
	wxBoxSizer* tolSizer = newd wxBoxSizer(wxHORIZONTAL);
	tolSizer->Add(newd wxStaticText(this, wxID_ANY, "Tolerance:"), 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 5);
	toleranceCtrl = newd wxSpinCtrl(this, wxID_ANY, "30", wxDefaultPosition, wxSize(60, -1), wxSP_ARROW_KEYS, 0, 765, 30);
	tolSizer->Add(toleranceCtrl, 0, wxALL, 2);
	rightSizer->Add(tolSizer, 0, wxEXPAND);

	// Filter
	wxBoxSizer* filterSizer = newd wxBoxSizer(wxHORIZONTAL);
	filterSizer->Add(newd wxStaticText(this, wxID_ANY, "Filter:"), 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 5);
	filterCtrl = newd wxTextCtrl(this, BITMAP_TO_MAP_FILTER, "", wxDefaultPosition, wxSize(150, -1));
	filterSizer->Add(filterCtrl, 1, wxALL, 2);
	rightSizer->Add(filterSizer, 0, wxEXPAND);

	// Color list
	colorListCtrl = newd wxListCtrl(this, BITMAP_TO_MAP_COLOR_LIST, wxDefaultPosition, wxSize(450, 250), wxLC_REPORT | wxLC_SINGLE_SEL);
	colorListCtrl->InsertColumn(0, "Color", wxLIST_FORMAT_LEFT, 80);
	colorListCtrl->InsertColumn(1, "Pixels", wxLIST_FORMAT_RIGHT, 60);
	colorListCtrl->InsertColumn(2, "Brush", wxLIST_FORMAT_LEFT, 150);
	colorListCtrl->InsertColumn(3, "Ignore", wxLIST_FORMAT_CENTER, 50);
	colorListCtrl->InsertColumn(4, "Hex", wxLIST_FORMAT_LEFT, 80);
	rightSizer->Add(colorListCtrl, 1, wxEXPAND | wxALL, 5);

	// Color list buttons
	wxBoxSizer* colorBtnSizer = newd wxBoxSizer(wxHORIZONTAL);
	colorBtnSizer->Add(newd wxButton(this, BITMAP_TO_MAP_DELETE_COLOR, "Delete Color"), 0, wxALL, 2);
	rightSizer->Add(colorBtnSizer, 0, wxALIGN_CENTER);

	// Action buttons
	wxBoxSizer* actionSizer = newd wxBoxSizer(wxHORIZONTAL);
	actionSizer->Add(newd wxButton(this, BITMAP_TO_MAP_PREVIEW, "Preview Colors"), 0, wxALL, 5);
	actionSizer->Add(newd wxButton(this, BITMAP_TO_MAP_GENERATE, "Generate"), 0, wxALL, 5);
	rightSizer->Add(actionSizer, 0, wxALIGN_CENTER);

	// Preset buttons
	wxBoxSizer* presetSizer = newd wxBoxSizer(wxHORIZONTAL);
	presetSizer->Add(newd wxButton(this, BITMAP_TO_MAP_SAVE_PRESET, "Save Preset"), 0, wxALL, 2);
	presetSizer->Add(newd wxButton(this, BITMAP_TO_MAP_LOAD_PRESET, "Load Preset"), 0, wxALL, 2);
	rightSizer->Add(presetSizer, 0, wxALIGN_CENTER);

	// Progress bar
	progressBar = newd wxGauge(this, wxID_ANY, 100, wxDefaultPosition, wxSize(-1, 20));
	rightSizer->Add(progressBar, 0, wxEXPAND | wxALL, 5);

	mainSizer->Add(rightSizer, 1, wxEXPAND);

	SetSizer(mainSizer);
	Layout();
	Centre(wxBOTH);
}

BitmapToMapWindow::~BitmapToMapWindow() {
}

void BitmapToMapWindow::OnClickBrowse(wxCommandEvent &event) {
	wxFileDialog dlg(this, "Select Image", "", "", "Image files (*.png;*.bmp;*.jpg;*.tga)|*.png;*.bmp;*.jpg;*.jpeg;*.tga|All files (*.*)|*.*", wxFD_OPEN | wxFD_FILE_MUST_EXIST);

	if (dlg.ShowModal() != wxID_OK) {
		return;
	}

	wxString path = dlg.GetPath();
	wxImage img;
	if (!img.LoadFile(path)) {
		g_gui.PopupDialog("Error", "Failed to load image: " + path, wxOK | wxICON_ERROR);
		return;
	}

	loadedImage = img;
	imageLoaded = true;

	imageInfoLabel->SetLabel(wxString::Format("%dx%d - %s", loadedImage.GetWidth(), loadedImage.GetHeight(), dlg.GetFilename()));

	imagePreview->SetBitmap(wxBitmap(loadedImage));
	previewPanel->SetVirtualSize(loadedImage.GetSize());
	previewPanel->Scroll(0, 0);

	detectColors();
	autoSuggestBrushes();
	populateColorList();
}

void BitmapToMapWindow::detectColors() {
	detectedColors.clear();

	if (!imageLoaded) {
		return;
	}

	int w = loadedImage.GetWidth();
	int h = loadedImage.GetHeight();
	bool hasAlpha = loadedImage.HasAlpha();
	unsigned char* data = loadedImage.GetData();
	unsigned char* alpha = hasAlpha ? loadedImage.GetAlpha() : nullptr;

	std::map<uint32_t, int> colorCounts;

	for (int i = 0; i < w * h; ++i) {
		if (hasAlpha && alpha && alpha[i] < 128) {
			continue;
		}

		uint8_t r = data[i * 3];
		uint8_t g = data[i * 3 + 1];
		uint8_t b = data[i * 3 + 2];
		uint32_t key = (uint32_t(r) << 16) | (uint32_t(g) << 8) | uint32_t(b);
		colorCounts[key]++;
	}

	if ((int)colorCounts.size() > MAX_COLORS) {
		wxMessageBox(
			wxString::Format("Image has %d unique colors (max %d).\n\n"
							 "Consider reducing colors in an image editor first.\n"
							 "Only the top %d most frequent colors will be loaded.",
							 (int)colorCounts.size(), MAX_COLORS, MAX_COLORS),
			"Too Many Colors", wxOK | wxICON_WARNING
		);
	}

	std::vector<std::pair<uint32_t, int>> sorted(colorCounts.begin(), colorCounts.end());
	std::sort(sorted.begin(), sorted.end(), [](const std::pair<uint32_t, int> &a, const std::pair<uint32_t, int> &b) {
		return a.second > b.second;
	});

	int limit = std::min((int)sorted.size(), MAX_COLORS);
	for (int i = 0; i < limit; ++i) {
		uint32_t key = sorted[i].first;
		uint8_t r = (key >> 16) & 0xFF;
		uint8_t g = (key >> 8) & 0xFF;
		uint8_t b = key & 0xFF;
		detectedColors.emplace_back(r, g, b, sorted[i].second);
	}
}

void BitmapToMapWindow::autoSuggestBrushes() {
	struct BrushColor {
		std::string name;
		uint8_t r, g, b;
	};

	std::vector<BrushColor> brushColors;

	const BrushMap &brushMap = g_brushes.getMap();
	for (auto it = brushMap.begin(); it != brushMap.end(); ++it) {
		Brush* brush = it->second;
		if (!brush->isGround()) {
			continue;
		}

		int lookId = brush->getLookID();
		if (lookId <= 0) {
			continue;
		}

		const ItemType &type = g_items.getItemType(lookId);
		if (type.id == 0 || !type.sprite) {
			continue;
		}

		uint16_t mc = type.sprite->getMiniMapColor();
		if (mc == 0) {
			continue;
		}

		wxColor rgb = colorFromEightBit(mc);
		brushColors.push_back({ brush->getName(), rgb.Red(), rgb.Green(), rgb.Blue() });
	}

	for (auto &dc : detectedColors) {
		if (dc.r == 0 && dc.g == 0 && dc.b == 0) {
			dc.ignore = true;
			continue;
		}

		int bestDist = 766;
		std::string bestBrush;

		for (const auto &bc : brushColors) {
			int dist = std::abs((int)dc.r - bc.r)
				+ std::abs((int)dc.g - bc.g)
				+ std::abs((int)dc.b - bc.b);
			if (dist < bestDist) {
				bestDist = dist;
				bestBrush = bc.name;
			}
		}

		if (!bestBrush.empty()) {
			dc.suggestedBrush = wxString(bestBrush);
		}
	}
}

void BitmapToMapWindow::populateColorList() {
	colorListCtrl->DeleteAllItems();

	wxString filter = filterCtrl->GetValue().Lower();

	for (size_t i = 0; i < detectedColors.size(); ++i) {
		const auto &dc = detectedColors[i];

		if (!filter.IsEmpty()) {
			wxString hex = dc.toHex().Lower();
			wxString brush = dc.suggestedBrush.Lower();
			if (hex.Find(filter) == wxNOT_FOUND && brush.Find(filter) == wxNOT_FOUND) {
				continue;
			}
		}

		long idx = colorListCtrl->InsertItem(colorListCtrl->GetItemCount(), "");
		colorListCtrl->SetItemData(idx, (long)i);

		colorListCtrl->SetItem(idx, 0, wxString::Format("(%d,%d,%d)", dc.r, dc.g, dc.b));
		colorListCtrl->SetItem(idx, 1, wxString::Format("%d", dc.pixelCount));
		colorListCtrl->SetItem(idx, 2, dc.suggestedBrush.IsEmpty() ? wxString("(none)") : dc.suggestedBrush);
		colorListCtrl->SetItem(idx, 3, dc.ignore ? "Yes" : "No");
		colorListCtrl->SetItem(idx, 4, dc.toHex());

		colorListCtrl->SetItemBackgroundColour(idx, wxColour(dc.r, dc.g, dc.b));
		int brightness = (dc.r * 299 + dc.g * 587 + dc.b * 114) / 1000;
		colorListCtrl->SetItemTextColour(idx, brightness > 128 ? *wxBLACK : *wxWHITE);
	}
}

void BitmapToMapWindow::OnColorListActivated(wxListEvent &event) {
	long sel = event.GetIndex();
	if (sel < 0) {
		return;
	}

	size_t dataIdx = (size_t)colorListCtrl->GetItemData(sel);
	if (dataIdx >= detectedColors.size()) {
		return;
	}

	auto &dc = detectedColors[dataIdx];

	wxArrayString brushNames;
	brushNames.Add("(ignore)");
	brushNames.Add("(none)");

	const BrushMap &brushMap = g_brushes.getMap();
	for (auto it = brushMap.begin(); it != brushMap.end(); ++it) {
		if (it->second->isGround()) {
			brushNames.Add(wxString(it->second->getName()));
		}
	}

	wxSingleChoiceDialog chooser(this, "Select brush for color " + dc.toHex(), "Choose Brush", brushNames);

	if (!dc.suggestedBrush.IsEmpty()) {
		int found = brushNames.Index(dc.suggestedBrush);
		if (found != wxNOT_FOUND) {
			chooser.SetSelection(found);
		}
	}

	if (chooser.ShowModal() == wxID_OK) {
		wxString chosen = chooser.GetStringSelection();
		if (chosen == "(ignore)") {
			dc.ignore = true;
			dc.suggestedBrush = "";
		} else if (chosen == "(none)") {
			dc.ignore = false;
			dc.suggestedBrush = "";
		} else {
			dc.ignore = false;
			dc.suggestedBrush = chosen;
		}
		populateColorList();
	}
}

void BitmapToMapWindow::OnClickDeleteColor(wxCommandEvent &event) {
	long sel = colorListCtrl->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	if (sel < 0) {
		return;
	}

	size_t dataIdx = (size_t)colorListCtrl->GetItemData(sel);
	if (dataIdx >= detectedColors.size()) {
		return;
	}

	detectedColors.erase(detectedColors.begin() + dataIdx);
	populateColorList();
}

void BitmapToMapWindow::OnFilterColors(wxCommandEvent &event) {
	populateColorList();
}

void BitmapToMapWindow::OnClickRotateLeft(wxCommandEvent &event) {
	if (!imageLoaded) {
		return;
	}
	loadedImage = loadedImage.Rotate90(false);
	imagePreview->SetBitmap(wxBitmap(loadedImage));
	previewPanel->SetVirtualSize(loadedImage.GetSize());
}

void BitmapToMapWindow::OnClickRotateRight(wxCommandEvent &event) {
	if (!imageLoaded) {
		return;
	}
	loadedImage = loadedImage.Rotate90(true);
	imagePreview->SetBitmap(wxBitmap(loadedImage));
	previewPanel->SetVirtualSize(loadedImage.GetSize());
}

void BitmapToMapWindow::OnClickFlip(wxCommandEvent &event) {
	if (!imageLoaded) {
		return;
	}
	loadedImage = loadedImage.Mirror(true);
	imagePreview->SetBitmap(wxBitmap(loadedImage));
}

void BitmapToMapWindow::OnClickCrop(wxCommandEvent &event) {
	wxMessageBox("Crop not implemented yet.", "Info", wxOK);
}

void BitmapToMapWindow::OnClickGenerate(wxCommandEvent &event) {
	if (!imageLoaded) {
		wxMessageBox("Please load an image first.", "Error", wxOK | wxICON_ERROR, this);
		return;
	}

	// Build color mappings from detected colors
	std::vector<ColorMapping> mappings;
	for (const auto &dc : detectedColors) {
		ColorMapping cm;
		cm.r = dc.r;
		cm.g = dc.g;
		cm.b = dc.b;
		cm.ignore = dc.ignore;
		cm.brushName = dc.suggestedBrush.IsEmpty() ? "" : dc.suggestedBrush.ToStdString();
		mappings.push_back(cm);
	}

	int tolerance = toleranceCtrl->GetValue();
	int offX = xOffsetCtrl->GetValue();
	int offY = yOffsetCtrl->GetValue();
	int offZ = zOffsetCtrl->GetValue();

	BitmapToMapConverter converter(editor);
	ConvertResult result = converter.convert(loadedImage, mappings, tolerance, offX, offY, offZ);

	if (result.success) {
		wxMessageBox(
			wxString::Format("Map generated!\n\nTiles placed: %d\nPixels skipped: %d", result.tilesPlaced, result.tilesSkipped),
			"Bitmap to Map", wxOK | wxICON_INFORMATION, this
		);
	} else {
		wxMessageBox(result.errorMessage, "Error", wxOK | wxICON_ERROR, this);
	}
}

void BitmapToMapWindow::OnClickPreview(wxCommandEvent &event) {
	wxMessageBox("Preview not implemented yet.", "Info", wxOK);
}

void BitmapToMapWindow::OnClickSavePreset(wxCommandEvent &event) {
	wxMessageBox("Save Preset not implemented yet.", "Info", wxOK);
}

void BitmapToMapWindow::OnClickLoadPreset(wxCommandEvent &event) {
	wxMessageBox("Load Preset not implemented yet.", "Info", wxOK);
}

void BitmapToMapWindow::recalculatePixelCounts() {
	if (!imageLoaded) {
		return;
	}

	for (auto &dc : detectedColors) {
		dc.pixelCount = 0;
	}

	unsigned char* data = loadedImage.GetData();
	bool hasAlpha = loadedImage.HasAlpha();
	unsigned char* alpha = hasAlpha ? loadedImage.GetAlpha() : nullptr;
	int w = loadedImage.GetWidth();
	int h = loadedImage.GetHeight();

	for (int i = 0; i < w * h; i++) {
		if (hasAlpha && alpha[i] < 128) {
			continue;
		}

		uint8_t r = data[i * 3];
		uint8_t g = data[i * 3 + 1];
		uint8_t b = data[i * 3 + 2];

		for (auto &dc : detectedColors) {
			if (dc.r == r && dc.g == g && dc.b == b) {
				dc.pixelCount++;
				break;
			}
		}
	}

	populateColorList();
}
