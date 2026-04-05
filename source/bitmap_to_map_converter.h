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
	BitmapToMapConverter(Editor& editor);
	~BitmapToMapConverter() = default;

	ConvertResult convert(
		const wxImage& image,
		const std::vector<ColorMapping>& mappings,
		int tolerance,
		int offsetX, int offsetY, int offsetZ
	);

private:
	Editor& editor;

	const ColorMapping* findMatchingColor(
		uint8_t r, uint8_t g, uint8_t b,
		const std::vector<ColorMapping>& mappings,
		int tolerance
	) const;
};

#endif // RME_BITMAP_TO_MAP_CONVERTER_H_
