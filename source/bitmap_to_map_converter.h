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

#ifndef RME_BITMAP_TO_MAP_CONVERTER_H_
#define RME_BITMAP_TO_MAP_CONVERTER_H_

#include "main.h"
#include <wx/image.h>
#include <string>
#include <vector>
#include <set>

class Editor;

struct ColorMapping {
	uint8_t r, g, b;
	std::string brushName;
	bool ignore;
};

struct ConvertResult {
	int tilesPlaced;
	int tilesSkipped;
	bool success;
	wxString errorMessage;
};

class BitmapToMapConverter {
public:
	BitmapToMapConverter(Editor &editor);
	~BitmapToMapConverter() = default;

	ConvertResult convert(
		const wxImage &image,
		const std::vector<ColorMapping> &mappings,
		int tolerance,
		int offsetX, int offsetY, int offsetZ
	);

private:
	Editor &editor;

	const ColorMapping* findMatchingColor(
		uint8_t r, uint8_t g, uint8_t b,
		const std::vector<ColorMapping> &mappings,
		int tolerance
	) const;
};

#endif // RME_BITMAP_TO_MAP_CONVERTER_H_
