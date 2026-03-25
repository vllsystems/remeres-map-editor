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
#include "npcs.h"
#include "npc_brush.h"

#include <wx/dir.h>
#include <fstream>

NpcDatabase g_npcs;

NpcType::NpcType() :
	missing(false),
	in_other_tileset(false),
	standard(false),
	name(""),
	brush(nullptr) {
	////
}

NpcType::NpcType(const NpcType &npc) :
	missing(npc.missing),
	in_other_tileset(npc.in_other_tileset),
	standard(npc.standard),
	name(npc.name),
	outfit(npc.outfit),
	brush(npc.brush) {
	////
}

NpcType &NpcType::operator=(const NpcType &npc) {
	missing = npc.missing;
	in_other_tileset = npc.in_other_tileset;
	standard = npc.standard;
	name = npc.name;
	outfit = npc.outfit;
	brush = npc.brush;
	return *this;
}

NpcType::~NpcType() {
	////
}

NpcType* NpcType::loadFromXML(pugi::xml_node node, wxArrayString &warnings) {
	pugi::xml_attribute attribute;
	if (!(attribute = node.attribute("name"))) {
		warnings.push_back("Couldn't read name tag of npc node.");
		return nullptr;
	}

	NpcType* npcType = newd NpcType();
	npcType->name = attribute.as_string();
	npcType->outfit.name = npcType->name;

	if ((attribute = node.attribute("looktype"))) {
		npcType->outfit.lookType = attribute.as_int();
		if (g_gui.gfx.getCreatureSprite(npcType->outfit.lookType) == nullptr) {
			warnings.push_back("Invalid npc \"" + wxstr(npcType->name) + "\" look type #" + std::to_string(npcType->outfit.lookType));
		}
	}

	if ((attribute = node.attribute("lookitem"))) {
		npcType->outfit.lookItem = attribute.as_int();
	}

	if ((attribute = node.attribute("lookaddon"))) {
		npcType->outfit.lookAddon = attribute.as_int();
	}

	if ((attribute = node.attribute("lookhead"))) {
		npcType->outfit.lookHead = attribute.as_int();
	}

	if ((attribute = node.attribute("lookbody"))) {
		npcType->outfit.lookBody = attribute.as_int();
	}

	if ((attribute = node.attribute("looklegs"))) {
		npcType->outfit.lookLegs = attribute.as_int();
	}

	if ((attribute = node.attribute("lookfeet"))) {
		npcType->outfit.lookFeet = attribute.as_int();
	}
	return npcType;
}

NpcType* NpcType::loadFromOTXML(const FileName &filename, pugi::xml_document &doc, wxArrayString &warnings) {
	ASSERT(doc != nullptr);

	pugi::xml_node node;
	if (!(node = doc.child("npc"))) {
		warnings.push_back("This file is not a npc file");
		return nullptr;
	}

	pugi::xml_attribute attribute;
	if (!(attribute = node.attribute("name"))) {
		warnings.push_back("Couldn't read name tag of npc node.");
		return nullptr;
	}

	NpcType* npcType = newd NpcType();
	npcType->name = nstr(filename.GetName());

	for (pugi::xml_node optionNode = node.first_child(); optionNode; optionNode = optionNode.next_sibling()) {
		if (as_lower_str(optionNode.name()) != "look") {
			continue;
		}

		if ((attribute = optionNode.attribute("type"))) {
			npcType->outfit.lookType = attribute.as_int();
		}

		if ((attribute = optionNode.attribute("item")) || (attribute = optionNode.attribute("lookex")) || (attribute = optionNode.attribute("typeex"))) {
			npcType->outfit.lookItem = attribute.as_int();
		}

		if ((attribute = optionNode.attribute("addon"))) {
			npcType->outfit.lookAddon = attribute.as_int();
		}

		if ((attribute = optionNode.attribute("head"))) {
			npcType->outfit.lookHead = attribute.as_int();
		}

		if ((attribute = optionNode.attribute("body"))) {
			npcType->outfit.lookBody = attribute.as_int();
		}

		if ((attribute = optionNode.attribute("legs"))) {
			npcType->outfit.lookLegs = attribute.as_int();
		}

		if ((attribute = optionNode.attribute("feet"))) {
			npcType->outfit.lookFeet = attribute.as_int();
		}
	}
	return npcType;
}

NpcDatabase::NpcDatabase() {
	////
}

NpcDatabase::~NpcDatabase() {
	clear();
}

void NpcDatabase::clear() {
	for (NpcMap::iterator iter = npcMap.begin(); iter != npcMap.end(); ++iter) {
		delete iter->second;
	}
	npcMap.clear();
}

NpcType* NpcDatabase::operator[](const std::string &name) {
	NpcMap::iterator iter = npcMap.find(as_lower_str(name));
	if (iter != npcMap.end()) {
		return iter->second;
	}
	return nullptr;
}

NpcType* NpcDatabase::addMissingNpcType(const std::string &name) {
	assert((*this)[name] == nullptr);

	NpcType* npcType = newd NpcType();
	npcType->name = name;
	npcType->missing = true;
	npcType->outfit.lookType = 130;

	npcMap.insert(std::make_pair(as_lower_str(name), npcType));
	return npcType;
}

NpcType* NpcDatabase::addNpcType(const std::string &name, const Outfit &outfit) {
	assert((*this)[name] == nullptr);

	NpcType* npcType = newd NpcType();
	npcType->name = name;
	npcType->missing = false;
	npcType->outfit = outfit;

	npcMap.insert(std::make_pair(as_lower_str(name), npcType));
	return npcType;
}

bool NpcDatabase::hasMissing() const {
	for (NpcMap::const_iterator iter = npcMap.begin(); iter != npcMap.end(); ++iter) {
		if (iter->second->missing) {
			return true;
		}
	}
	return false;
}

bool NpcDatabase::loadFromXML(const FileName &filename, bool standard, wxString &error, wxArrayString &warnings) {
	pugi::xml_document doc;
	pugi::xml_parse_result result = doc.load_file(filename.GetFullPath().mb_str());
	if (!result) {
		error = "Couldn't open file \"" + filename.GetFullName() + "\", invalid format?";
		return false;
	}

	pugi::xml_node node = doc.child("npcs");
	if (!node) {
		error = "Invalid file signature, this file is not a valid npc file.";
		return false;
	}

	for (pugi::xml_node npcNode = node.first_child(); npcNode; npcNode = npcNode.next_sibling()) {
		if (as_lower_str(npcNode.name()) != "npc") {
			continue;
		}

		NpcType* npcType = NpcType::loadFromXML(npcNode, warnings);
		if (npcType) {
			npcType->standard = standard;
			if ((*this)[npcType->name]) {
				warnings.push_back("Duplicate npc with name \"" + wxstr(npcType->name) + "\"! Discarding...");
				delete npcType;
			} else {
				npcMap[as_lower_str(npcType->name)] = npcType;
			}
		}
	}
	return true;
}

bool NpcDatabase::importXMLFromOT(const FileName &filename, wxString &error, wxArrayString &warnings) {
	pugi::xml_document doc;
	pugi::xml_parse_result result = doc.load_file(filename.GetFullPath().mb_str());
	if (!result) {
		error = "Couldn't open file \"" + filename.GetFullName() + "\", invalid format?";
		return false;
	}

	pugi::xml_node node;
	if ((node = doc.child("npc"))) {
		NpcType* npcType = NpcType::loadFromOTXML(filename, doc, warnings);
		if (npcType) {
			NpcType* current = (*this)[npcType->name];

			if (current) {
				*current = *npcType;
				delete npcType;
			} else {
				npcMap[as_lower_str(npcType->name)] = npcType;

				Tileset* tileSet = nullptr;
				tileSet = g_materials.tilesets["NPCs"];
				ASSERT(tileSet != nullptr);

				Brush* brush = newd NpcBrush(npcType);
				g_brushes.addBrush(brush);

				TilesetCategory* tileSetCategory = tileSet->getCategory(TILESET_NPC);
				tileSetCategory->brushlist.push_back(brush);
			}
		}
	} else {
		error = "This is not valid OT npc data file.";
		return false;
	}
	return true;
}

bool NpcDatabase::saveToXML(const FileName &filename) {
	pugi::xml_document doc;

	pugi::xml_node decl = doc.prepend_child(pugi::node_declaration);
	decl.append_attribute("version") = "1.0";

	pugi::xml_node npcNodes = doc.append_child("npcs");
	for (const auto &npcEntry : npcMap) {
		NpcType* npcType = npcEntry.second;
		if (!npcType->standard) {
			pugi::xml_node npcNode = npcNodes.append_child("npc");

			npcNode.append_attribute("name") = npcType->name.c_str();
			npcNode.append_attribute("type") = "npc";

			const Outfit &outfit = npcType->outfit;
			npcNode.append_attribute("looktype") = outfit.lookType;
			npcNode.append_attribute("lookitem") = outfit.lookItem;
			npcNode.append_attribute("lookaddon") = outfit.lookAddon;
			npcNode.append_attribute("lookhead") = outfit.lookHead;
			npcNode.append_attribute("lookbody") = outfit.lookBody;
			npcNode.append_attribute("looklegs") = outfit.lookLegs;
			npcNode.append_attribute("lookfeet") = outfit.lookFeet;
		}
	}
	return doc.save_file(filename.GetFullPath().mb_str(), "\t", pugi::format_default, pugi::encoding_utf8);
}

wxArrayString NpcDatabase::getMissingNpcNames() const {
	wxArrayString missingNpcs;
	for (const auto &ncpEntry : npcMap) {
		if (ncpEntry.second->missing) {
			missingNpcs.Add(ncpEntry.second->name);
		}
	}
	return missingNpcs;
}

static std::string parseLuaCreateCall(const std::string &content, const std::string &funcName) {
	size_t pos = content.find(funcName);
	if (pos == std::string::npos) {
		return "";
	}
	pos += funcName.size();
	while (pos < content.size() && (content[pos] == ' ' || content[pos] == '\t')) {
		++pos;
	}
	if (pos >= content.size() || content[pos] != '(') {
		return "";
	}
	++pos;
	while (pos < content.size() && (content[pos] == ' ' || content[pos] == '\t' || content[pos] == '\n' || content[pos] == '\r')) {
		++pos;
	}
	if (pos >= content.size() || content[pos] != '"') {
		return "";
	}
	++pos;
	size_t nameStart = pos;
	while (pos < content.size() && content[pos] != '"') {
		++pos;
	}
	if (pos >= content.size()) {
		return "";
	}
	return content.substr(nameStart, pos - nameStart);
}

static std::string parseLuaLocalString(const std::string &content, const std::string &varName) {
	std::string pattern = "local " + varName;
	size_t pos = content.find(pattern);
	if (pos == std::string::npos) {
		return "";
	}
	pos += pattern.size();
	while (pos < content.size() && (content[pos] == ' ' || content[pos] == '\t')) {
		++pos;
	}
	if (pos >= content.size() || content[pos] != '=') {
		return "";
	}
	++pos;
	while (pos < content.size() && (content[pos] == ' ' || content[pos] == '\t')) {
		++pos;
	}
	if (pos >= content.size() || content[pos] != '"') {
		return "";
	}
	++pos;
	size_t nameStart = pos;
	while (pos < content.size() && content[pos] != '"') {
		++pos;
	}
	if (pos >= content.size()) {
		return "";
	}
	return content.substr(nameStart, pos - nameStart);
}

static int parseLuaField(const std::string &block, const std::string &key) {
	size_t searchFrom = 0;
	while (searchFrom < block.size()) {
		size_t pos = block.find(key, searchFrom);
		if (pos == std::string::npos) {
			return -1;
		}
		size_t afterKey = pos + key.size();
		if (afterKey < block.size() && (std::isalnum(block[afterKey]) || block[afterKey] == '_')) {
			searchFrom = afterKey;
			continue;
		}
		pos = afterKey;
		while (pos < block.size() && (block[pos] == ' ' || block[pos] == '\t')) {
			++pos;
		}
		if (pos >= block.size() || block[pos] != '=') {
			searchFrom = pos;
			continue;
		}
		++pos;
		while (pos < block.size() && (block[pos] == ' ' || block[pos] == '\t')) {
			++pos;
		}
		int val = 0;
		bool found = false;
		while (pos < block.size() && block[pos] >= '0' && block[pos] <= '9') {
			val = val * 10 + (block[pos] - '0');
			found = true;
			++pos;
		}
		if (found) {
			return val;
		}
		searchFrom = pos;
	}
	return -1;
}

bool NpcDatabase::loadFromLuaDir(const wxString &directory, wxString &error, wxArrayString &warnings) {
	if (directory.IsEmpty() || !wxDir::Exists(directory)) {
		return true;
	}

	wxArrayString luaFiles;
	wxDir::GetAllFiles(directory, &luaFiles, "*.lua", wxDIR_FILES | wxDIR_DIRS);

	int fileCount = 0;
	for (const auto &filePath : luaFiles) {
		if (++fileCount % 50 == 0) {
			wxSafeYield();
		}
		std::ifstream ifs(filePath.ToStdString(), std::ios::binary | std::ios::ate);
		if (!ifs.is_open()) {
			warnings.push_back("Could not open: " + filePath);
			continue;
		}
		auto fileSize = ifs.tellg();
		ifs.seekg(0, std::ios::beg);
		std::string content(fileSize, '\0');
		ifs.read(&content[0], fileSize);
		ifs.close();

		std::string name = parseLuaCreateCall(content, "Game.createNpcType");
		if (name.empty()) {
			name = parseLuaLocalString(content, "internalNpcName");
		}
		if (name.empty()) {
			continue;
		}

		if ((*this)[name]) {
			continue;
		}

		std::string outfitBlock;
		size_t outfitPos = content.find(".outfit");
		if (outfitPos == std::string::npos) {
			outfitPos = content.find(":outfit(");
		}
		if (outfitPos == std::string::npos) {
			continue;
		}
		size_t braceStart = content.find('{', outfitPos);
		size_t braceEnd = content.find('}', braceStart);
		if (braceStart == std::string::npos || braceEnd == std::string::npos) {
			continue;
		}
		outfitBlock = content.substr(braceStart, braceEnd - braceStart + 1);

		NpcType* npcType = newd NpcType();
		npcType->name = name;
		npcType->outfit.name = name;
		npcType->standard = false;

		int val;
		if ((val = parseLuaField(outfitBlock, "lookType")) >= 0) {
			npcType->outfit.lookType = val;
		}
		if ((val = parseLuaField(outfitBlock, "lookTypeEx")) >= 0) {
			npcType->outfit.lookItem = val;
		}
		if ((val = parseLuaField(outfitBlock, "lookHead")) >= 0) {
			npcType->outfit.lookHead = val;
		}
		if ((val = parseLuaField(outfitBlock, "lookBody")) >= 0) {
			npcType->outfit.lookBody = val;
		}
		if ((val = parseLuaField(outfitBlock, "lookLegs")) >= 0) {
			npcType->outfit.lookLegs = val;
		}
		if ((val = parseLuaField(outfitBlock, "lookFeet")) >= 0) {
			npcType->outfit.lookFeet = val;
		}
		if ((val = parseLuaField(outfitBlock, "lookAddons")) >= 0) {
			npcType->outfit.lookAddon = val;
		}
		if ((val = parseLuaField(outfitBlock, "lookMount")) >= 0) {
			npcType->outfit.lookMount = val;
		}

		npcMap[as_lower_str(npcType->name)] = npcType;
	}
	return true;
}
