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
#include "lua_script_manager.h"

#include <wx/dir.h>
#include <wx/filename.h>
#include <wx/stdpaths.h>

#include <algorithm>
#include <ranges>

bool LuaScriptManager::initialize() {
	if (initialized) {
		return true;
	}

	if (!engine.initialize()) {
		lastError = engine.getLastError();
		return false;
	}

	initialized = true;

	// Discover and execute scripts automatically
	discoverScripts();

	return true;
}

void LuaScriptManager::shutdown() {
	if (!initialized) {
		return;
	}

	engine.shutdown();
	initialized = false;
}

std::string LuaScriptManager::getScriptsDirectory() const {
	wxFileName execPath(wxStandardPaths::Get().GetExecutablePath());
	wxString scriptsDir = execPath.GetPath() + wxFileName::GetPathSeparator() + "scripts";
	return scriptsDir.ToStdString();
}

void LuaScriptManager::discoverScripts() {
	scripts.clear();
	std::string scriptsDir = getScriptsDirectory();
	scanDirectory(scriptsDir);
	std::ranges::sort(scripts);
	runAutoScripts();
}

void LuaScriptManager::scanDirectory(const std::string &directory) {
	wxDir dir(directory);
	if (!dir.IsOpened()) {
		return;
	}

	const std::string sep(1, wxFileName::GetPathSeparator());

	wxString name;
	bool cont = dir.GetFirst(&name, "*.lua", wxDIR_FILES);
	while (cont) {
		// Skip files starting with underscore (convention for tool scripts)
		if (!name.IsEmpty() && name[0] == '_') {
			cont = dir.GetNext(&name);
			continue;
		}

		std::string filepath = directory + sep + name.ToStdString();
		scripts.push_back(filepath);
		cont = dir.GetNext(&name);
	}
}

void LuaScriptManager::runAutoScripts() {
	for (const auto &scriptPath : scripts) {
		if (!executeScript(scriptPath)) {
			wxLogWarning("Failed to execute script '%s': %s", scriptPath.c_str(), lastError.c_str());
		}
	}
}

bool LuaScriptManager::executeScript(const std::string &filepath) {
	if (!initialized) {
		lastError = "Script manager not initialized";
		return false;
	}

	bool result = engine.executeFile(filepath);
	if (!result) {
		lastError = engine.getLastError();
	}
	return result;
}
