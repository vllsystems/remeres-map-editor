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

#ifndef RME_BITMAP_TO_MAP_WINDOW_H_
#define RME_BITMAP_TO_MAP_WINDOW_H_

#include "main.h"
#include <wx/listctrl.h>
#include <wx/spinctrl.h>

class Editor;

struct DetectedColor {
	uint8_t r, g, b;
	int pixelCount;
	wxString suggestedBrush;
	bool ignore;

	DetectedColor() :
		r(0), g(0), b(0), pixelCount(0), ignore(false) { }
	DetectedColor(uint8_t r, uint8_t g, uint8_t b, int count) :
		r(r), g(g), b(b), pixelCount(count), ignore(false) { }

	wxString toHex() const {
		return wxString::Format("#%02X%02X%02X", r, g, b);
	}

	bool matches(uint8_t or_, uint8_t og, uint8_t ob, int tolerance) const {
		int dist = std::abs((int)r - or_) + std::abs((int)g - og) + std::abs((int)b - ob);
		return dist <= tolerance;
	}
};

enum {
	BITMAP_TO_MAP_BROWSE = wxID_HIGHEST + 1,
	BITMAP_TO_MAP_GENERATE,
	BITMAP_TO_MAP_PREVIEW,
	BITMAP_TO_MAP_ROTATE_LEFT,
	BITMAP_TO_MAP_ROTATE_RIGHT,
	BITMAP_TO_MAP_FLIP,
	BITMAP_TO_MAP_CROP,
	BITMAP_TO_MAP_SAVE_PRESET,
	BITMAP_TO_MAP_LOAD_PRESET,
	BITMAP_TO_MAP_DELETE_COLOR,
	BITMAP_TO_MAP_FILTER,
	BITMAP_TO_MAP_MATCH_MODE,
	BITMAP_TO_MAP_TOLERANCE,
	BITMAP_TO_MAP_COLOR_LIST,
	BITMAP_TO_MAP_INSTRUCTIONS,
};

class BitmapToMapWindow : public wxDialog {
public:
	BitmapToMapWindow(wxWindow* parent, Editor &editor);
	virtual ~BitmapToMapWindow();

private:
	void OnClickBrowse(wxCommandEvent &event);
	void OnClickGenerate(wxCommandEvent &event);
	void OnClickPreview(wxCommandEvent &event);
	void OnClickRotateLeft(wxCommandEvent &event);
	void OnClickRotateRight(wxCommandEvent &event);
	void OnClickFlip(wxCommandEvent &event);
	void OnClickCrop(wxCommandEvent &event);
	void OnPreviewMouseWheel(wxMouseEvent &event);
	void OnPreviewMouseMove(wxMouseEvent &event);
	void generateColorizedPreview();
	void OnClickSavePreset(wxCommandEvent &event);
	void OnClickLoadPreset(wxCommandEvent &event);
	void OnClickDeleteColor(wxCommandEvent &event);
	void OnClickInstructions(wxCommandEvent &event);
	void OnMatchModeChanged(wxCommandEvent &event);
	void OnToleranceChanged(wxSpinEvent &event);
	void OnFilterColors(wxCommandEvent &event);
	void OnColorListActivated(wxListEvent &event);

	void detectColors();
	void autoSuggestBrushes();
	void populateColorList();
	void recalculatePixelCounts();

	Editor &editor;
	wxImage loadedImage;
	bool imageLoaded;

	// Controls
	wxStaticBitmap* imagePreview;
	wxScrolledWindow* previewPanel;
	wxListCtrl* colorListCtrl;
	wxTextCtrl* filterCtrl;
	wxSpinCtrl* toleranceCtrl;
	wxSpinCtrl* xOffsetCtrl;
	wxSpinCtrl* yOffsetCtrl;
	wxSpinCtrl* zOffsetCtrl;
	wxStaticText* imageInfoLabel;
	wxGauge* progressBar;
	wxChoice* matchModeChoice;
	wxChoice* scaleChoice;
	wxSpinCtrl* cropXCtrl;
	wxSpinCtrl* cropYCtrl;
	wxSpinCtrl* cropWCtrl;
	wxSpinCtrl* cropHCtrl;
	wxStaticText* pixelInfoLabel;
	double zoomLevel;
	wxImage originalImage;

	// Data
	std::vector<DetectedColor> detectedColors;
	static const int MAX_COLORS = 256;

	DECLARE_EVENT_TABLE()
};

#endif // RME_BITMAP_TO_MAP_WINDOW_H_
