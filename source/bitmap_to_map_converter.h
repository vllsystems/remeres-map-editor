#ifndef RME_BITMAP_TO_MAP_CONVERTER_H_
#define RME_BITMAP_TO_MAP_CONVERTER_H_

#include "main.h"

class Editor;

class BitmapToMapConverter {
public:
	BitmapToMapConverter(Editor& editor);
	~BitmapToMapConverter() = default;

private:
	Editor& editor;
};

#endif // RME_BITMAP_TO_MAP_CONVERTER_H_
