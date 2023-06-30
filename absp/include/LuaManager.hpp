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
#include "ADQ36_FWDAQ.hpp"

extern "C" {
#include "swigluarun.h"

// Declaring the init function of the lua wrapped module.
// It needs to be in an extern block, so the compiler does not mess up its name
extern int luaopen_digitizers(lua_State* L);
}

class LuaManager {
private:
    int verbosity;
    lua_State* Lua; // the Lua state

    unsigned int counter_updates;
    unsigned int counter_runs;

    // SWIG types
    swig_type_info *SWIG_ABCD_Digitizer_t;
    swig_type_info *SWIG_ABCD_ADQ214_t;
    swig_type_info *SWIG_ABCD_ADQ412_t;
    swig_type_info *SWIG_ABCD_ADQ14_FWDAQ_t;
    swig_type_info *SWIG_ABCD_ADQ14_FWPD_t;
    swig_type_info *SWIG_ABCD_ADQ36_FWDAQ_t;

public:
    LuaManager(int V = 1) : verbosity(V), counter_updates(0), counter_runs(0) {
        // Create a new state
        Lua = luaL_newstate();

        if (verbosity > 0) {
            std::cout << "LuaManager::LuaManager() ";
            std::cout << "Stack length: " << lua_gettop(Lua) << "; ";
            std::cout << std::endl;
        }

        // Load the standard libraries
        luaL_openlibs(Lua);

        // Load the SWIG wrapped module
        // FIXME: Check whether we should use luaL_requiref() instead of calling directly
        luaopen_digitizers(Lua);

        // Create a new table in which to store the digitizers
        lua_newtable(Lua);

        // Set a name to the topmost entry in the interpreter stack
        lua_setglobal(Lua, "digitizer_instances");

        lua_newtable(Lua);
        lua_setglobal(Lua, "current_state");

        lua_pushinteger(Lua, DIGITIZER_SUCCESS);
        lua_setglobal(Lua, "DIGITIZER_SUCCESS");

        lua_pushinteger(Lua, DIGITIZER_FAILURE);
        lua_setglobal(Lua, "DIGITIZER_FAILURE");

        // I put these here, otherwise they give a segfault if put in the
        // update_digitizers() method
        // FIXME: Understand why is it the case...
        std::cout << "LuaManager::LuaManager() Type query; " << std::endl;
        SWIG_ABCD_Digitizer_t = SWIG_TypeQuery(Lua, "ABCD::Digitizer *");
        SWIG_ABCD_ADQ214_t = SWIG_TypeQuery(Lua, "ABCD::ADQ214 *");
        SWIG_ABCD_ADQ412_t = SWIG_TypeQuery(Lua, "ABCD::ADQ412 *");
        SWIG_ABCD_ADQ14_FWDAQ_t = SWIG_TypeQuery(Lua, "ABCD::ADQ14_FWDAQ *");
        SWIG_ABCD_ADQ14_FWPD_t = SWIG_TypeQuery(Lua, "ABCD::ADQ14_FWPD *");
        SWIG_ABCD_ADQ36_FWDAQ_t = SWIG_TypeQuery(Lua, "ABCD::ADQ36_FWDAQ *");

        if (verbosity > 0) {
            run_string("print(\"LUA INTERPRETER DEBUG OUTPUT: LuaManager::LuaManager() Finished\");");

            dump_stack();
        }
    }

    ~LuaManager() {
        if (verbosity > 0) {
            run_string("print(\"LUA INTERPRETER DEBUG OUTPUT: LuaManager::~LuaManager()\");");
        }

        // Close the lua state
        lua_close(Lua);
    }

    // A helper function that prints the stack length and content
    void dump_stack() {
        // Get the stack length
        const int top = lua_gettop(Lua);

        std::cout << "LuaManager::dump_stack() ";
        std::cout << "Stack length: " << top << "; ";
        std::cout << "Stack: [";

        for (int i = 1; i <= top; i++) {
            const int t = lua_type(Lua, i);

            switch (t) {
                case LUA_TSTRING:
                    std::cout << lua_tostring(Lua, i);
                    break;
                case LUA_TBOOLEAN:
                    std::cout << (lua_toboolean(Lua, i) ? "true" : "false");
                    break;
                case LUA_TNUMBER:
                    std::cout << lua_tonumber(Lua, i);
                    break;
                default:
                    std::cout << lua_typename(Lua, t);
                    break;
            }
            std::cout << ", ";
        }
        std::cout << "]; " << std::endl;
    }

    void update_digitizers(std::vector<ABCD::Digitizer*> &digitizers) {
        counter_updates += 1;

        lua_pushinteger(Lua, counter_updates);
        lua_setglobal(Lua, "counter_updates");

        if (verbosity > 0) {
            std::cout << "LuaManager::update_digitizers() ";
            std::cout << "Number of digitizers: " << digitizers.size() << "; ";
            std::cout << std::endl;
        }

        lua_getglobal(Lua, "digitizer_instances");

        for (auto &digitizer: digitizers) {
            if (verbosity > 0) {
                std::cout << "LuaManager::update_digitizers() ";
                std::cout << "Found digitizer: " << digitizer->GetModel() << "; ";
                std::cout << std::endl;
            }

            if (digitizer->GetModel() == "ADQ214") {
                ABCD::ADQ214 *digitizer_adq = reinterpret_cast<ABCD::ADQ214*>(digitizer);

                // Create a new userdata object that wraps the digitizer and
                // push it onto the stack
                SWIG_NewPointerObj(Lua, digitizer_adq, SWIG_ABCD_ADQ214_t, 0);

                lua_setfield(Lua, -2, digitizer->GetName().c_str());

            } else if (digitizer->GetModel() == "ADQ412") {
                ABCD::ADQ412 *digitizer_adq = reinterpret_cast<ABCD::ADQ412*>(digitizer);

                SWIG_NewPointerObj(Lua, digitizer_adq, SWIG_ABCD_ADQ412_t, 0);

                lua_setfield(Lua, -2, digitizer->GetName().c_str());

            } else if (digitizer->GetModel() == "ADQ14_FWDAQ") {
                ABCD::ADQ14_FWDAQ *digitizer_adq = reinterpret_cast<ABCD::ADQ14_FWDAQ*>(digitizer);

                SWIG_NewPointerObj(Lua, digitizer_adq, SWIG_ABCD_ADQ14_FWDAQ_t, 0);

                lua_setfield(Lua, -2, digitizer->GetName().c_str());

            } else if (digitizer->GetModel() == "ADQ14_FWPD") {
                ABCD::ADQ14_FWPD *digitizer_adq = reinterpret_cast<ABCD::ADQ14_FWPD*>(digitizer);

                SWIG_NewPointerObj(Lua, digitizer_adq, SWIG_ABCD_ADQ14_FWPD_t, 0);

                lua_setfield(Lua, -2, digitizer->GetName().c_str());
            } else if (digitizer->GetModel() == "ADQ36_FWDAQ") {
                ABCD::ADQ36_FWDAQ *digitizer_adq = reinterpret_cast<ABCD::ADQ36_FWDAQ*>(digitizer);

                SWIG_NewPointerObj(Lua, digitizer_adq, SWIG_ABCD_ADQ36_FWDAQ_t, 0);

                lua_setfield(Lua, -2, digitizer->GetName().c_str());

            } else {
                if (verbosity > 0) {
                    std::cout << "LuaManager::update_digitizers() ";
                    std::cout << WRITE_YELLOW << "WARNING:" << WRITE_NC << " Using the generic Digitizer interface for the unexpected digitizer: " << digitizer->GetModel() << " name: " << digitizer->GetName() << "; ";
                    std::cout << std::endl;
                }

                SWIG_NewPointerObj(Lua, digitizer, SWIG_ABCD_Digitizer_t, 0);

                lua_setfield(Lua, -2, digitizer->GetName().c_str());
            }
        }

        // It is important to pop the digitizer_instances from the stack,
        // otherwise it would pile-up in it multiple times.
        lua_pop(Lua, 1);

        if (verbosity > 0) {
            run_string("print(\"LUA INTERPRETER DEBUG OUTPUT: LuaManager::update_digitizers() Finished\");");

            dump_stack();
        }
    }

    void run_string(std::string source) {
        counter_runs += 1;

        lua_pushinteger(Lua, counter_runs);
        lua_setglobal(Lua, "counter_runs");

        if (verbosity > 0) {
            std::cout << "LuaManager::run_string() ";
            std::cout << "Running string; ";
            std::cout << std::endl;
        }

        const int result = luaL_dostring(Lua, source.c_str());

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

    void run_script(unsigned int state_ID,
                    std::string state_description,
                    std::string when,
                    std::string source)
    {
        if (verbosity > 0) {
            std::cout << "LuaManager::run_script() ";
            std::cout << "Running script; ";
            std::cout << "Current state ID: " << state_ID << "; ";
            std::cout << "Current state description: " << state_description << "; ";
            std::cout << std::endl;
        }

        lua_getglobal(Lua, "current_state");

        lua_pushinteger(Lua, state_ID);
        lua_setfield(Lua, -2, "ID");

        lua_pushstring(Lua, state_description.c_str());
        lua_setfield(Lua, -2, "description");

        lua_pushstring(Lua, when.c_str());
        lua_setfield(Lua, -2, "when");

        lua_pop(Lua, 1);

        run_string(source);

        if (verbosity > 0) {
            run_string("print(\"LUA INTERPRETER DEBUG OUTPUT: LuaManager::run_script() Finished\");");

            dump_stack();
        }
    }
};

#endif
