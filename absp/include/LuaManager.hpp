#ifndef LUAMANAGER_HPP
#define LUAMANAGER_HPP

#include <iostream>

#include <lua.hpp>

#include "ADQ_descriptions.hpp"
#include "ADQ_utilities.hpp"

#include "Digitizer.hpp"

#include "ADQ214.hpp"
#include "ADQ412.hpp"
#include "ADQ14_FWDAQ.hpp"
#include "ADQ14_FWPD.hpp"

extern "C" {
#include "swigluarun.h"

// Declaring the init function of the lua wrapped module.
// It needs to be in an extern block, so the compiler does not mess up its name
extern int luaopen_digitizers(lua_State* L);
extern int luaopen_digitizers(lua_State* L);
}

class LuaManager {
private:
    int verbosity;
    lua_State* Lua; // the Lua state

public:
    LuaManager(int V = 0) : verbosity(V) {
        // Create a new state
        Lua = luaL_newstate();

        // Load the standard libraries
        luaL_openlibs(Lua);

        // Load the SWIG wrapped module
        // FIXME: Check whether we should use luaL_requiref() instead of calling directly
        luaopen_digitizers(Lua);

        // Create a new table in which to store the digitizers
        lua_newtable(Lua);
        // Set a name to the topmost entry in the interpreter stack
        lua_setglobal(Lua, "digitizers");

        run_script("print(\"LUA INTERPRETER DEBUG: LuaManager::LuaManager()\");");
    }

    ~LuaManager() {
        run_script("print(\"LUA INTERPRETER DEBUG: LuaManager::~LuaManager()\");");

        // Close the state
        lua_close(Lua);
    }

    void update_digitizers(std::vector<ABCD::Digitizer*> digitizers) {
        run_script("print(\"LUA INTERPRETER DEBUG: LuaManager::update_digitizers()\");");

        lua_getglobal(Lua, "digitizers");

        for (auto &digitizer: digitizers) {
            if (verbosity > 0) {
                std::cout << "LuaManager::update_digitizers() ";
                std::cout << "Found digitizer: " << digitizer->GetModel() << "; ";
                std::cout << std::endl;
            }

            if (digitizer->GetModel() == "ADQ214") {
                swig_type_info *type = SWIG_TypeQuery(Lua, "ADQ214");

                ABCD::ADQ214 *digitizer_adq = reinterpret_cast<ABCD::ADQ214*>(digitizer);

                // Create a new userdata object that wraps the digitizer and
                // push it onto the stack
                SWIG_NewPointerObj(Lua, digitizer_adq, type, 0);

                lua_setfield(Lua, -2, digitizer->GetModel().c_str());

            } else if (digitizer->GetModel() == "ADQ412") {
                swig_type_info *type = SWIG_TypeQuery(Lua, "ADQ412");

                ABCD::ADQ412 *digitizer_adq = reinterpret_cast<ABCD::ADQ412*>(digitizer);

                SWIG_NewPointerObj(Lua, digitizer_adq, type, 0);

                lua_setfield(Lua, -2, digitizer->GetModel().c_str());

            } else if (digitizer->GetModel() == "ADQ14_FWDAQ") {
                swig_type_info *type = SWIG_TypeQuery(Lua, "ADQ14_FWDAQ");

                ABCD::ADQ14_FWDAQ *digitizer_adq = reinterpret_cast<ABCD::ADQ14_FWDAQ*>(digitizer);

                SWIG_NewPointerObj(Lua, digitizer_adq, type, 0);

                lua_setfield(Lua, -2, digitizer->GetModel().c_str());

            } else if (digitizer->GetModel() == "ADQ14_FWPD") {
                swig_type_info *type = SWIG_TypeQuery(Lua, "ADQ14_FWPD");

                ABCD::ADQ14_FWPD *digitizer_adq = reinterpret_cast<ABCD::ADQ14_FWPD*>(digitizer);

                SWIG_NewPointerObj(Lua, digitizer_adq, type, 0);

                lua_setfield(Lua, -2, digitizer->GetModel().c_str());

            }
        }
    }

    void run_script(std::string source) {
        const int result = luaL_dostring(Lua, source.c_str());

        if (verbosity > 0) {
            if (result != LUA_OK) { // there was an error
                switch (result) {
                    case LUA_ERRSYNTAX:
                        std::cout << WRITE_RED << "LUA ERROR" << WRITE_NC << ": Syntax error: ";
                        break;
                    case LUA_ERRMEM:                                                                                             std::cout << WRITE_RED << "LUA ERROR" << WRITE_NC << ": Memory allocation error: ";
                        break;
                    case LUA_ERRFILE:
                        std::cout << WRITE_RED << "LUA ERROR" << WRITE_NC << ": File error: ";
                        break;
                    case LUA_ERRRUN:
                        std::cout << WRITE_RED << "LUA ERROR" << WRITE_NC << ": Runtime error: ";
                        break;
                    case LUA_ERRERR:                                                                                             std::cout << WRITE_RED << "LUA ERROR" << WRITE_NC << ": Error handler error: ";
                        break;
                    default:
                        std::cout << WRITE_RED << "LUA ERROR" << WRITE_NC << ": Unknown error: ";
                        break;
                }

                // Get the error message from the Lua stack and print it
                std::cout << lua_tostring(Lua, -1) << std::endl;
                // Pop the error message from the Lua stack
                lua_pop(Lua, 1);
            }
        }
    }
};

#endif
