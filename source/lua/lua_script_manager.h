//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_LUA_SCRIPT_MANAGER_H
#define RME_LUA_SCRIPT_MANAGER_H

#include "lua_engine.h"

#include <string>
#include <vector>

class LuaScriptManager {
public:
	static LuaScriptManager &getInstance() {
		static LuaScriptManager instance;
		return instance;
	}

	bool initialize();
	void shutdown();
	bool isInitialized() const {
		return initialized;
	}

	LuaEngine &getEngine() {
		return engine;
	}
	std::string getLastError() const {
		return lastError;
	}

private:
	LuaScriptManager() = default;
	~LuaScriptManager() = default;
	LuaScriptManager(const LuaScriptManager &) = delete;
	LuaScriptManager &operator=(const LuaScriptManager &) = delete;

	LuaEngine engine;
	std::string lastError;
	bool initialized = false;
	std::vector<std::string> scripts;

	std::string getScriptsDirectory() const;
	void discoverScripts();
	void scanDirectory(const std::string &directory);
	void runAutoScripts();
	bool executeScript(const std::string &filepath);
};

inline LuaScriptManager &getLuaManager() {
	return LuaScriptManager::getInstance();
}
#define g_luaScripts (getLuaManager())

#endif // RME_LUA_SCRIPT_MANAGER_H
