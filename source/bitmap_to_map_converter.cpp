#include "main.h"
#include "bitmap_to_map_converter.h"
#include "editor.h"
#include "map.h"
#include "tile.h"
#include "item.h"
#include "ground_brush.h"
#include "brush.h"
#include "action.h"
#include "gui.h"
#include "settings.h"

BitmapToMapConverter::BitmapToMapConverter(Editor& editor) :
	editor(editor) {
}

const ColorMapping* BitmapToMapConverter::findMatchingColor(
	uint8_t r, uint8_t g, uint8_t b,
	const std::vector<ColorMapping>& mappings,
	int tolerance
) const {
	const ColorMapping* bestMatch = nullptr;
	int bestDistance = tolerance + 1;

	for (const auto& mapping : mappings) {
		int distance = std::abs((int)r - (int)mapping.r)
					 + std::abs((int)g - (int)mapping.g)
					 + std::abs((int)b - (int)mapping.b);
		if (distance <= tolerance && distance < bestDistance) {
			bestDistance = distance;
			bestMatch = &mapping;
		}
	}
	return bestMatch;
}

ConvertResult BitmapToMapConverter::convert(
	const wxImage& image,
	const std::vector<ColorMapping>& mappings,
	int tolerance,
	int offsetX, int offsetY, int offsetZ
) {
	ConvertResult result;
	result.tilesPlaced = 0;
	result.tilesSkipped = 0;
	result.success = false;

	if (!image.IsOk()) {
		result.errorMessage = "Invalid image.";
		return result;
	}

	if (mappings.empty()) {
		result.errorMessage = "No color mappings defined.";
		return result;
	}

	Map& map = editor.getMap();
	int imgWidth = image.GetWidth();
	int imgHeight = image.GetHeight();
	int totalPixels = imgWidth * imgHeight;

	g_gui.CreateLoadBar("Generating map from bitmap...");

	// Phase 1: Place ground tiles
	BatchAction* batch = editor.createBatch(ACTION_DRAW);
	Action* action = editor.createAction(batch);

	// Track positions that need borderizing (placed tiles + neighbors)
	std::set<Position> borderPositions;

	const unsigned char* imgData = image.GetData();
	bool hasAlpha = image.HasAlpha();
	const unsigned char* alphaData = hasAlpha ? image.GetAlpha() : nullptr;

	int pixelsDone = 0;
	for (int py = 0; py < imgHeight; py++) {
		for (int px = 0; px < imgWidth; px++) {
			if (pixelsDone % 4096 == 0) {
				g_gui.SetLoadDone(static_cast<int32_t>(50.0 * pixelsDone / totalPixels));
			}
			pixelsDone++;

			// Skip transparent pixels
			if (hasAlpha && alphaData[py * imgWidth + px] < 128) {
				result.tilesSkipped++;
				continue;
			}

			int idx = (py * imgWidth + px) * 3;
			uint8_t r = imgData[idx];
			uint8_t g_color = imgData[idx + 1];
			uint8_t b_color = imgData[idx + 2];

			const ColorMapping* mapping = findMatchingColor(r, g_color, b_color, mappings, tolerance);
			if (!mapping || mapping->ignore || mapping->brushName.empty()) {
				result.tilesSkipped++;
				continue;
			}

			Brush* brush = g_brushes.getBrush(mapping->brushName);
			if (!brush || !brush->isGround()) {
				result.tilesSkipped++;
				continue;
			}

			GroundBrush* groundBrush = brush->asGround();

			int mapX = px + offsetX;
			int mapY = py + offsetY;
			int mapZ = offsetZ;

			if (mapX < 0 || mapY < 0 || mapZ < 0 || mapZ > 15) {
				result.tilesSkipped++;
				continue;
			}

			Position pos(mapX, mapY, mapZ);

			TileLocation* location = map.createTileL(pos);
			Tile* tile = location->get();
			Tile* new_tile = nullptr;

			if (tile) {
				new_tile = tile->deepCopy(map);
				new_tile->cleanBorders();
			} else {
				new_tile = map.allocator(location);
			}

			groundBrush->draw(&map, new_tile, nullptr);
			action->addChange(newd Change(new_tile));
			result.tilesPlaced++;

			// Track neighbors for borderizing
			for (int dy = -1; dy <= 1; dy++) {
				for (int dx = -1; dx <= 1; dx++) {
					int bx = mapX + dx;
					int by = mapY + dy;
					if (bx >= 0 && by >= 0) {
						borderPositions.insert(Position(bx, by, mapZ));
					}
				}
			}
		}
	}

	// Commit ground tiles to map
	batch->addAndCommitAction(action);

	// Phase 2: Borderize all affected tiles
	if (!borderPositions.empty()) {
		action = editor.createAction(batch);

		int bordersDone = 0;
		int totalBorders = static_cast<int>(borderPositions.size());

		for (const Position& pos : borderPositions) {
			if (bordersDone % 4096 == 0) {
				g_gui.SetLoadDone(static_cast<int32_t>(50 + 49.0 * bordersDone / totalBorders));
			}
			bordersDone++;

			TileLocation* location = map.createTileL(pos);
			Tile* tile = location->get();

			if (tile) {
				Tile* new_tile = tile->deepCopy(map);
				new_tile->borderize(&map);
				action->addChange(newd Change(new_tile));
			} else {
				Tile* new_tile = map.allocator(location);
				new_tile->borderize(&map);
				if (new_tile->size() > 0) {
					action->addChange(newd Change(new_tile));
				} else {
					delete new_tile;
				}
			}
		}

		batch->addAndCommitAction(action);
	}

	// Phase 3: Finalize
	editor.addBatch(batch);
	editor.updateActions();

	g_gui.DestroyLoadBar();
	g_gui.RefreshView();

	result.success = true;
	return result;
}
