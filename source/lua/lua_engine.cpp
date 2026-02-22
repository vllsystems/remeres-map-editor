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
#include "lua_engine.h"

#include <fstream>
#include <sstream>

LuaEngine::LuaEngine() :
	initialized(false) {
}

LuaEngine::~LuaEngine() {
	shutdown();
}

bool LuaEngine::initialize() {
	if (initialized) {
		return true;
	}

	try {
		lua.open_libraries(
			sol::lib::base,
			sol::lib::package,
			sol::lib::coroutine,
			sol::lib::string,
			sol::lib::table,
			sol::lib::math,
			sol::lib::utf8,
			sol::lib::os
		);

		setupSandbox();
		registerBaseLibraries();

		initialized = true;
		return true;
	} catch (const sol::error &e) {
		lastError = std::string("Failed to initialize Lua engine: ") + e.what();
		return false;
	} catch (const std::exception &e) {
		lastError = std::string("Failed to initialize Lua engine: ") + e.what();
		return false;
	}
}

void LuaEngine::shutdown() {
	if (!initialized) {
		return;
	}

	lua = sol::state();
	initialized = false;
}

void LuaEngine::setupSandbox() {
	if (lua["os"].valid()) {
		lua["os"]["execute"] = sol::nil;
		lua["os"]["exit"] = sol::nil;
		lua["os"]["remove"] = sol::nil;
		lua["os"]["rename"] = sol::nil;
		lua["os"]["tmpname"] = sol::nil;
		lua["os"]["getenv"] = sol::nil;
		lua["os"]["setlocale"] = sol::nil;
	}

	lua["io"] = sol::nil;

	if (lua["package"].valid()) {
		lua["package"]["loadlib"] = sol::nil;
		lua["package"]["path"] = sol::nil;
		lua["package"]["cpath"] = sol::nil;
		lua["package"]["searchpath"] = sol::nil;

		if (lua["package"]["searchers"].valid()) {
			sol::table searchers = lua["package"]["searchers"];
			searchers[2] = sol::nil; // Disable Lua file searcher
			searchers[3] = sol::nil;
			searchers[4] = sol::nil;
		}
	}

	lua["dofile"] = [this](const std::string &filename, sol::this_state s) -> bool {
		if (this->currentScriptDir.empty()) {
			throw sol::error("dofile: Script directory not set. Cannot resolve relative path.");
		}

		if (filename.find(":") != std::string::npos || (filename.size() > 0 && (filename[0] == '/' || filename[0] == '\\'))) {
			throw sol::error("dofile: Absolute paths are not allowed. Use paths relative to the script.");
		}
		if (filename.find("..") != std::string::npos) {
			throw sol::error("dofile: Directory traversal ('..') is not allowed.");
		}

		std::string cleanFilename = filename;
		if (cleanFilename.substr(0, 2) == "./" || cleanFilename.substr(0, 2) == ".\\") {
			cleanFilename = cleanFilename.substr(2);
		}

		std::string fullPath = this->currentScriptDir + "/" + cleanFilename;
		return this->executeFile(fullPath);
	};

	lua["loadfile"] = [this](const std::string &filename, sol::this_state s) -> sol::object {
		sol::state_view lua(s);

		if (this->currentScriptDir.empty()) {
			throw sol::error("loadfile: Script directory not set. Cannot resolve relative path.");
		}

		if (filename.find(":") != std::string::npos || (filename.size() > 0 && (filename[0] == '/' || filename[0] == '\\'))) {
			throw sol::error("loadfile: Absolute paths are not allowed. Use paths relative to the script.");
		}
		if (filename.find("..") != std::string::npos) {
			throw sol::error("loadfile: Directory traversal ('..') is not allowed.");
		}

		std::string cleanFilename = filename;
		if (cleanFilename.substr(0, 2) == "./" || cleanFilename.substr(0, 2) == ".\\") {
			cleanFilename = cleanFilename.substr(2);
		}

		std::string fullPath = this->currentScriptDir + "/" + cleanFilename;

		// Extract directory for the chunk
		std::string chunkDir;
		size_t lastSlash = fullPath.find_last_of("/\\");
		if (lastSlash != std::string::npos) {
			chunkDir = fullPath.substr(0, lastSlash);
		} else {
			chunkDir = ".";
		}

		// Load the file
		sol::load_result loaded = lua.load_file(fullPath);
		if (!loaded.valid()) {
			sol::error err = loaded;
			throw err;
		}

		// Create a wrapper function in Lua that manages currentScriptDir
		sol::function wrapperFactory = lua.script(R"(
			return function(chunk, dir, engine)
				return function(...)
					local saved = engine.currentScriptDir
					engine.currentScriptDir = dir
					local results = {pcall(chunk, ...)}
					engine.currentScriptDir = saved
					if not results[1] then
						error(results[2])
					end
					return select(2, table.unpack(results))
				end
			end
		)");

		// Call the factory to create the wrapped chunk
		return wrapperFactory(loaded, chunkDir, this);
	};

	// **ADIÇÃO CRUCIAL: Expor LuaEngine para Lua**
	lua.new_usertype<LuaEngine>("LuaEngine", "currentScriptDir", &LuaEngine::currentScriptDir);

	try {
		lua.script(R"(
			local old_load = load
			_G.load = function(chunk, chunkname, mode, env)
				if mode and mode ~= "t" then
					error("Secure Mode: Binary chunks are disabled.")
				end
				return old_load(chunk, chunkname, "t", env)
			end
		)");
	} catch (const std::exception &e) {
		lastError = std::string("Failed to install secure load patch: ") + e.what();
		throw;
	} catch (...) {
		lastError = "Failed to install secure load patch: Unknown error";
		throw;
	}
}

void LuaEngine::handlePrint(sol::variadic_args va) {
	std::ostringstream oss;
	bool first = true;
	for (auto v : va) {
		if (!first) {
			oss << "\t";
		}
		first = false;

		sol::state_view lua(v.lua_state());
		sol::function tostring = lua["tostring"];
		std::string str = tostring(v);
		oss << str;
	}

	std::string output = oss.str();

	if (printCallback) {
		printCallback(output);
	} else {
		std::cout << "[Lua] " << output << std::endl;
	}
}

void LuaEngine::registerBaseLibraries() {
	lua["print"] = [this](sol::variadic_args va) {
		handlePrint(va);
	};
}

void LuaEngine::setPrintCallback(PrintCallback callback) {
	printCallback = callback;

	lua["print"] = [this](sol::variadic_args va) {
		handlePrint(va);
	};
}

bool LuaEngine::executeFile(const std::string &filepath) {
	if (!initialized) {
		lastError = "Lua engine not initialized";
		return false;
	}

	// Save current script directory for RAII restoration
	std::string savedScriptDir = currentScriptDir;

	// RAII guard to restore currentScriptDir on all exit paths
	struct ScopeGuard {
		LuaEngine* engine;
		std::string savedDir;
		~ScopeGuard() noexcept {
			engine->currentScriptDir = savedDir;
		}
	};
	ScopeGuard guard { this, savedScriptDir };

	try {
		std::string scriptDir;
		size_t lastSlash = filepath.find_last_of("/\\");
		if (lastSlash != std::string::npos) {
			scriptDir = filepath.substr(0, lastSlash);
		} else {
			scriptDir = ".";
		}
		currentScriptDir = scriptDir;

		sol::load_result loaded = lua.load_file(filepath);
		if (!loaded.valid()) {
			sol::error err = loaded;
			lastError = std::string("Failed to load script '") + filepath + "': " + err.what();
			return false;
		}

		sol::protected_function script = loaded;
		sol::protected_function_result result = script();

		if (!result.valid()) {
			sol::error err = result;
			lastError = std::string("Error executing script '") + filepath + "': " + err.what();
			return false;
		}

		return true;
	} catch (const sol::error &e) {
		lastError = std::string("Exception executing script '") + filepath + "': " + e.what();
		return false;
	} catch (const std::exception &e) {
		lastError = std::string("Exception executing script '") + filepath + "': " + e.what();
		return false;
	}
}

bool LuaEngine::executeString(const std::string &code, const std::string &chunkName) {
	if (!initialized) {
		lastError = "Lua engine not initialized";
		return false;
	}

	try {
		sol::load_result loaded = lua.load(code, chunkName);
		if (!loaded.valid()) {
			sol::error err = loaded;
			lastError = std::string("Failed to load code: ") + err.what();
			return false;
		}

		sol::protected_function script = loaded;
		sol::protected_function_result result = script();

		if (!result.valid()) {
			sol::error err = result;
			lastError = std::string("Error executing code: ") + err.what();
			return false;
		}

		return true;
	} catch (const sol::error &e) {
		lastError = std::string("Exception executing code: ") + e.what();
		return false;
	} catch (const std::exception &e) {
		lastError = std::string("Exception executing code: ") + e.what();
		return false;
	}
}
