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

#include "gui.h"
#include "materials.h"
#include "brush.h"
#include "monsters.h"
#include "monster_brush.h"

MonsterDatabase g_monsters;

MonsterType::MonsterType() :
	missing(false),
	in_other_tileset(false),
	standard(false),
	name(""),
	brush(nullptr) {
	////
}

MonsterType::MonsterType(const MonsterType &ct) :
	missing(ct.missing),
	in_other_tileset(ct.in_other_tileset),
	standard(ct.standard),
	name(ct.name),
	outfit(ct.outfit),
	brush(ct.brush) {
	////
}

MonsterType &MonsterType::operator=(const MonsterType &ct) {
	missing = ct.missing;
	in_other_tileset = ct.in_other_tileset;
	standard = ct.standard;
	name = ct.name;
	outfit = ct.outfit;
	brush = ct.brush;
	return *this;
}

MonsterType::~MonsterType() {
	////
}

MonsterType* MonsterType::loadFromXML(pugi::xml_node node, wxArrayString &warnings) {
	pugi::xml_attribute attribute;
	if (!(attribute = node.attribute("name"))) {
		warnings.push_back("Couldn't read name tag of monster node.");
		return nullptr;
	}

	MonsterType* ct = newd MonsterType();
	ct->name = attribute.as_string();
	ct->outfit.name = ct->name;

	if ((attribute = node.attribute("looktype"))) {
		ct->outfit.lookType = attribute.as_int();
		if (g_gui.gfx.getCreatureSprite(ct->outfit.lookType) == nullptr) {
			warnings.push_back("Invalid monster \"" + wxstr(ct->name) + "\" look type #" + std::to_string(ct->outfit.lookType));
		}
	}

	if ((attribute = node.attribute("lookitem"))) {
		ct->outfit.lookItem = attribute.as_int();
	}

	if ((attribute = node.attribute("lookmount"))) {
		ct->outfit.lookMount = attribute.as_int();
	}

	if ((attribute = node.attribute("lookaddon"))) {
		ct->outfit.lookAddon = attribute.as_int();
	}

	if ((attribute = node.attribute("lookhead"))) {
		ct->outfit.lookHead = attribute.as_int();
	}

	if ((attribute = node.attribute("lookbody"))) {
		ct->outfit.lookBody = attribute.as_int();
	}

	if ((attribute = node.attribute("looklegs"))) {
		ct->outfit.lookLegs = attribute.as_int();
	}

	if ((attribute = node.attribute("lookfeet"))) {
		ct->outfit.lookFeet = attribute.as_int();
	}
	return ct;
}

MonsterType* MonsterType::loadFromOTXML(const FileName &filename, pugi::xml_document &doc, wxArrayString &warnings) {
	ASSERT(doc != nullptr);
	pugi::xml_node node;
	if (!(node = doc.child("monster"))) {
		warnings.push_back("This file is not a monster file");
		return nullptr;
	}

	pugi::xml_attribute attribute;
	if (!(attribute = node.attribute("name"))) {
		warnings.push_back("Couldn't read name tag of monster node.");
		return nullptr;
	}

	MonsterType* ct = newd MonsterType();
	ct->name = attribute.as_string();
	ct->outfit.name = ct->name;

	for (pugi::xml_node optionNode = node.first_child(); optionNode; optionNode = optionNode.next_sibling()) {
		if (as_lower_str(optionNode.name()) != "look") {
			continue;
		}

		if ((attribute = optionNode.attribute("type"))) {
			ct->outfit.lookType = attribute.as_int();
		}

		if ((attribute = optionNode.attribute("item")) || (attribute = optionNode.attribute("lookex")) || (attribute = optionNode.attribute("typeex"))) {
			ct->outfit.lookItem = attribute.as_int();
		}

		if ((attribute = optionNode.attribute("mount"))) {
			ct->outfit.lookMount = attribute.as_int();
		}

		if ((attribute = optionNode.attribute("addon"))) {
			ct->outfit.lookAddon = attribute.as_int();
		}

		if ((attribute = optionNode.attribute("head"))) {
			ct->outfit.lookHead = attribute.as_int();
		}

		if ((attribute = optionNode.attribute("body"))) {
			ct->outfit.lookBody = attribute.as_int();
		}

		if ((attribute = optionNode.attribute("legs"))) {
			ct->outfit.lookLegs = attribute.as_int();
		}

		if ((attribute = optionNode.attribute("feet"))) {
			ct->outfit.lookFeet = attribute.as_int();
		}
	}
	return ct;
}

MonsterDatabase::MonsterDatabase() {
	////
}

MonsterDatabase::~MonsterDatabase() {
	clear();
}

void MonsterDatabase::clear() {
	for (MonsterMap::iterator iter = monster_map.begin(); iter != monster_map.end(); ++iter) {
		delete iter->second;
	}
	monster_map.clear();
}

MonsterType* MonsterDatabase::operator[](const std::string &name) {
	if (name.empty()) {
		return nullptr;
	}

	MonsterMap::iterator iter = monster_map.find(as_lower_str(name));
	if (iter != monster_map.end()) {
		return iter->second;
	}
	return nullptr;
}

MonsterType* MonsterDatabase::addMissingMonsterType(const std::string &name) {
	assert((*this)[name] == nullptr);

	MonsterType* ct = newd MonsterType();
	ct->name = name;
	ct->missing = true;
	ct->outfit.lookType = 130;

	monster_map.insert(std::make_pair(as_lower_str(name), ct));
	return ct;
}

MonsterType* MonsterDatabase::addMonsterType(const std::string &name, const Outfit &outfit) {
	assert((*this)[name] == nullptr);

	MonsterType* ct = newd MonsterType();
	ct->name = name;
	ct->missing = false;
	ct->outfit = outfit;

	monster_map.insert(std::make_pair(as_lower_str(name), ct));
	return ct;
}

bool MonsterDatabase::hasMissing() const {
	for (MonsterMap::const_iterator iter = monster_map.begin(); iter != monster_map.end(); ++iter) {
		if (iter->second->missing) {
			return true;
		}
	}
	return false;
}

bool MonsterDatabase::loadFromXML(const FileName &filename, bool standard, wxString &error, wxArrayString &warnings) {
	pugi::xml_document doc;
	pugi::xml_parse_result result = doc.load_file(filename.GetFullPath().mb_str());
	if (!result) {
		error = "Couldn't open file \"" + filename.GetFullName() + "\", invalid format?";
		return false;
	}

	pugi::xml_node node = doc.child("monsters");
	if (!node) {
		error = "Invalid file signature, this file is not a valid monsters file.";
		return false;
	}

	for (pugi::xml_node monsterNode = node.first_child(); monsterNode; monsterNode = monsterNode.next_sibling()) {
		if (as_lower_str(monsterNode.name()) != "monster") {
			continue;
		}

		MonsterType* monsterType = MonsterType::loadFromXML(monsterNode, warnings);
		if (monsterType) {
			monsterType->standard = standard;
			if ((*this)[monsterType->name]) {
				warnings.push_back("Duplicate monster type name \"" + wxstr(monsterType->name) + "\"! Discarding...");
				delete monsterType;
			} else {
				monster_map[as_lower_str(monsterType->name)] = monsterType;
			}
		}
	}
	return true;
}

bool MonsterDatabase::importXMLFromOT(const FileName &filename, wxString &error, wxArrayString &warnings) {
	pugi::xml_document doc;
	pugi::xml_parse_result result = doc.load_file(filename.GetFullPath().mb_str());
	if (!result) {
		error = "Couldn't open file \"" + filename.GetFullName() + "\", invalid format?";
		return false;
	}

	pugi::xml_node node;
	if ((node = doc.child("monsters"))) {
		for (pugi::xml_node monsterNode = node.first_child(); monsterNode; monsterNode = monsterNode.next_sibling()) {
			if (as_lower_str(monsterNode.name()) != "monster") {
				continue;
			}

			pugi::xml_attribute attribute;
			if (!(attribute = monsterNode.attribute("file"))) {
				continue;
			}

			FileName monsterFile(filename);
			monsterFile.SetFullName(wxString(attribute.as_string(), wxConvUTF8));

			pugi::xml_document monsterDoc;
			pugi::xml_parse_result monsterResult = monsterDoc.load_file(monsterFile.GetFullPath().mb_str());
			if (!monsterResult) {
				continue;
			}

			MonsterType* monsterType = MonsterType::loadFromOTXML(monsterFile, monsterDoc, warnings);
			if (monsterType) {
				MonsterType* current = (*this)[monsterType->name];
				if (current) {
					*current = *monsterType;
					delete monsterType;
				} else {
					monster_map[as_lower_str(monsterType->name)] = monsterType;

					Tileset* tileSet = nullptr;
					tileSet = g_materials.tilesets["Monsters"];
					ASSERT(tileSet != nullptr);

					Brush* brush = newd MonsterBrush(monsterType);
					g_brushes.addBrush(brush);

					TilesetCategory* tileSetCategory = tileSet->getCategory(TILESET_MONSTER);
					tileSetCategory->brushlist.push_back(brush);
				}
			}
		}
	} else if ((node = doc.child("monster"))) {
		MonsterType* monsterType = MonsterType::loadFromOTXML(filename, doc, warnings);
		if (monsterType) {
			MonsterType* current = (*this)[monsterType->name];

			if (current) {
				*current = *monsterType;
				delete monsterType;
			} else {
				monster_map[as_lower_str(monsterType->name)] = monsterType;

				Tileset* tileSet = nullptr;
				tileSet = g_materials.tilesets["Monsters"];
				ASSERT(tileSet != nullptr);

				Brush* brush = newd MonsterBrush(monsterType);
				g_brushes.addBrush(brush);

				TilesetCategory* tileSetCategory = tileSet->getCategory(TILESET_MONSTER);
				tileSetCategory->brushlist.push_back(brush);
			}
		}
	} else {
		error = "This is not valid OT monster data file.";
		return false;
	}
	return true;
}

bool MonsterDatabase::saveToXML(const FileName &filename) {
	pugi::xml_document doc;

	pugi::xml_node decl = doc.prepend_child(pugi::node_declaration);
	decl.append_attribute("version") = "1.0";

	pugi::xml_node monsterNodes = doc.append_child("monsters");
	for (const auto &monsterEntry : monster_map) {
		MonsterType* monsterType = monsterEntry.second;
		if (!monsterType->standard) {
			pugi::xml_node monsterNode = monsterNodes.append_child("monster");

			monsterNode.append_attribute("name") = monsterType->name.c_str();
			monsterNode.append_attribute("type") = "monster";

			const Outfit &outfit = monsterType->outfit;
			monsterNode.append_attribute("looktype") = outfit.lookType;
			monsterNode.append_attribute("lookitem") = outfit.lookItem;
			monsterNode.append_attribute("lookaddon") = outfit.lookAddon;
			monsterNode.append_attribute("lookhead") = outfit.lookHead;
			monsterNode.append_attribute("lookbody") = outfit.lookBody;
			monsterNode.append_attribute("looklegs") = outfit.lookLegs;
			monsterNode.append_attribute("lookfeet") = outfit.lookFeet;
		}
	}
	return doc.save_file(filename.GetFullPath().mb_str(), "\t", pugi::format_default, pugi::encoding_utf8);
}

bool MonsterDatabase::loadFromLuaDir(const FileName &directory, bool standard, wxString &error, wxArrayString &warnings) {
	wxString dirPath = directory.GetFullPath();
	if (!wxDirExists(dirPath)) {
		error = "Monster Lua directory not found: " + dirPath;
		return false;
	}

	wxArrayString luaFiles;
	wxDir::GetAllFiles(dirPath, &luaFiles, "*.lua", wxDIR_FILES | wxDIR_DIRS);

	if (luaFiles.IsEmpty()) {
		warnings.push_back("No .lua monster files found in: " + dirPath);
		return true;
	}

	int loadedCount = 0;
	for (const auto &luaFile : luaFiles) {
		MonsterType* monsterType = MonsterType::loadFromLuaFile(luaFile.ToStdString(), warnings);
		if (monsterType) {
			monsterType->standard = standard;
			if ((*this)[monsterType->name]) {
				// Skip duplicates silently - Canary has many files
				delete monsterType;
			} else {
				monster_map[as_lower_str(monsterType->name)] = monsterType;
				loadedCount++;
			}
		}
	}

	spdlog::info("Loaded {} monsters from Lua files in: {}", loadedCount, dirPath.ToStdString());
	return true;
}

std::string MonsterType::extractQuotedString(const std::string &line, const std::string &pattern) {
	size_t pos = line.find(pattern);
	if (pos == std::string::npos) {
		return "";
	}
	size_t start = line.find('"', pos + pattern.length());
	if (start == std::string::npos) {
		return "";
	}
	start++;
	size_t end = line.find('"', start);
	if (end == std::string::npos) {
		return "";
	}
	return line.substr(start, end - start);
}

int MonsterType::extractLuaInt(const std::string &content, const std::string &fieldName) {
	size_t pos = content.find(fieldName);
	while (pos != std::string::npos) {
		// Make sure this is the field name and not part of another word
		if (pos > 0 && (std::isalnum(content[pos - 1]) || content[pos - 1] == '_')) {
			pos = content.find(fieldName, pos + 1);
			continue;
		}
		size_t eqPos = content.find('=', pos + fieldName.length());
		if (eqPos == std::string::npos) {
			break;
		}
		// Check there's only whitespace between field name and '='
		bool onlySpaces = true;
		for (size_t i = pos + fieldName.length(); i < eqPos; i++) {
			if (!std::isspace(static_cast<unsigned char>(content[i]))) {
				onlySpaces = false;
				break;
			}
		}
		if (!onlySpaces) {
			pos = content.find(fieldName, pos + 1);
			continue;
		}
		// Find the number after '='
		size_t numStart = eqPos + 1;
		while (numStart < content.size() && std::isspace(static_cast<unsigned char>(content[numStart]))) {
			numStart++;
		}
		if (numStart < content.size() && (std::isdigit(static_cast<unsigned char>(content[numStart])) || content[numStart] == '-')) {
			size_t numEnd = numStart + 1;
			while (numEnd < content.size() && std::isdigit(static_cast<unsigned char>(content[numEnd]))) {
				numEnd++;
			}
			try {
				return std::stoi(content.substr(numStart, numEnd - numStart));
			} catch (...) {
				return 0;
			}
		}
		pos = content.find(fieldName, pos + 1);
	}
	return 0;
}

MonsterType* MonsterType::loadFromLuaFile(const std::string &filepath, wxArrayString &warnings) {
	std::ifstream file(filepath);
	if (!file.is_open()) {
		return nullptr;
	}

	std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	file.close();

	if (content.empty()) {
		return nullptr;
	}

	// Extract monster name from Game.createMonsterType("Name")
	std::string name;
	size_t createPos = content.find("Game.createMonsterType(");
	if (createPos != std::string::npos) {
		size_t quoteStart = content.find('"', createPos);
		if (quoteStart != std::string::npos) {
			quoteStart++;
			size_t quoteEnd = content.find('"', quoteStart);
			if (quoteEnd != std::string::npos) {
				name = content.substr(quoteStart, quoteEnd - quoteStart);
			}
		}
	}

	if (name.empty()) {
		return nullptr;
	}

	// Find the outfit block
	size_t outfitPos = content.find(".outfit");
	if (outfitPos == std::string::npos) {
		return nullptr;
	}

	size_t braceStart = content.find('{', outfitPos);
	if (braceStart == std::string::npos) {
		return nullptr;
	}

	// Find matching closing brace
	int braceCount = 1;
	size_t braceEnd = braceStart + 1;
	while (braceEnd < content.size() && braceCount > 0) {
		if (content[braceEnd] == '{') {
			braceCount++;
		} else if (content[braceEnd] == '}') {
			braceCount--;
		}
		braceEnd++;
	}

	std::string outfitBlock = content.substr(braceStart, braceEnd - braceStart);

	MonsterType* ct = newd MonsterType();
	ct->name = name;
	ct->outfit.name = ct->name;

	ct->outfit.lookType = extractLuaInt(outfitBlock, "lookType");
	ct->outfit.lookHead = extractLuaInt(outfitBlock, "lookHead");
	ct->outfit.lookBody = extractLuaInt(outfitBlock, "lookBody");
	ct->outfit.lookLegs = extractLuaInt(outfitBlock, "lookLegs");
	ct->outfit.lookFeet = extractLuaInt(outfitBlock, "lookFeet");
	ct->outfit.lookAddon = extractLuaInt(outfitBlock, "lookAddons");
	ct->outfit.lookMount = extractLuaInt(outfitBlock, "lookMount");

	// lookTypeEx in Canary maps to lookItem in RME
	int lookTypeEx = extractLuaInt(outfitBlock, "lookTypeEx");
	if (lookTypeEx > 0) {
		ct->outfit.lookItem = lookTypeEx;
	}

	// Validate: must have either lookType or lookItem
	if (ct->outfit.lookType == 0 && ct->outfit.lookItem == 0) {
		warnings.push_back("Monster \"" + wxstr(name) + "\" has no lookType or lookTypeEx in: " + wxstr(filepath));
		delete ct;
		return nullptr;
	}

	return ct;
}

wxArrayString MonsterDatabase::getMissingMonsterNames() const {
	wxArrayString missingMonsters;
	for (const auto &monsterEntry : monster_map) {
		if (monsterEntry.second->missing) {
			missingMonsters.Add(monsterEntry.second->name);
		}
	}
	return missingMonsters;
}
