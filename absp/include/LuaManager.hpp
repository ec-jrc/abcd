#ifndef LUAMANAGER_HPP
#define LUAMANAGER_HPP

// For std::this_thread::sleep_for
#include <thread>
#include <chrono>

#include <lua.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

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
extern int luaopen_ADQAPI(lua_State* L);
extern int luaopen_digitizers(lua_State* L);
}

extern std::shared_ptr<spdlog::logger> absp_logger_console;
extern std::shared_ptr<spdlog::logger> absp_logger_error;

class LuaManager {
private:
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
    LuaManager() : counter_updates(0), counter_runs(0) {
        if (absp_logger_console == nullptr) {
            absp_logger_console = spdlog::stdout_color_mt("lua");

            absp_logger_console->set_level(spdlog::level::info);
        }
        if (absp_logger_error == nullptr) {
            absp_logger_error = spdlog::stderr_color_mt("lua_error");
        }

        // Create a new state
        Lua = luaL_newstate();

        absp_logger_console->debug("Stack length: {}", lua_gettop(Lua));

        // Load the standard libraries
        luaL_openlibs(Lua);

        // Load the SWIG wrapped modules
        // FIXME: Check whether we should use luaL_requiref() instead of calling directly
        luaopen_ADQAPI(Lua);
        luaopen_digitizers(Lua);

        lua_pushcfunction(Lua, LuaManager::sleep);
        lua_setglobal(Lua, "sleep");

        lua_pushcfunction(Lua, LuaManager::abcd_log);
        lua_setglobal(Lua, "abcd_log");

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
        absp_logger_console->debug("LuaManager::LuaManager() Type query");

        SWIG_ABCD_Digitizer_t = SWIG_TypeQuery(Lua, "ABCD::Digitizer *");
        SWIG_ABCD_ADQ214_t = SWIG_TypeQuery(Lua, "ABCD::ADQ214 *");
        SWIG_ABCD_ADQ412_t = SWIG_TypeQuery(Lua, "ABCD::ADQ412 *");
        SWIG_ABCD_ADQ14_FWDAQ_t = SWIG_TypeQuery(Lua, "ABCD::ADQ14_FWDAQ *");
        SWIG_ABCD_ADQ14_FWPD_t = SWIG_TypeQuery(Lua, "ABCD::ADQ14_FWPD *");
        SWIG_ABCD_ADQ36_FWDAQ_t = SWIG_TypeQuery(Lua, "ABCD::ADQ36_FWDAQ *");

        if (absp_logger_console->should_log(spdlog::level::debug))
        {
            run_string("abcd_log(\"LUA INTERPRETER DEBUG OUTPUT: LuaManager::LuaManager() Finished\");");

            dump_stack();
        }
    }

    ~LuaManager() {
        if (absp_logger_console->should_log(spdlog::level::info))
        {
            run_string("abcd_log(\"LUA INTERPRETER DEBUG OUTPUT: LuaManager::~LuaManager()\");");
        }

        // Close the lua state
        lua_close(Lua);
    }

    // A helper function that prints the stack length and content
    void dump_stack() {
        const int top = lua_gettop(Lua);

        std::string stack_content;
        for (int i = 1; i <= top; i++) {
            const int t = lua_type(Lua, i);

            switch (t) {
                case LUA_TSTRING:
                    stack_content += lua_tostring(Lua, i);
                    break;
                case LUA_TBOOLEAN:
                    stack_content += (lua_toboolean(Lua, i) ? "true" : "false");
                    break;
                case LUA_TNUMBER:
                    stack_content += std::to_string(lua_tonumber(Lua, i));
                    break;
                default:
                    stack_content += lua_typename(Lua, t);
                    break;
            }
            stack_content += ", ";
        }

        absp_logger_console->debug("Stack length: {}; Stack: [{}]", top, stack_content);
    }

    void update_digitizers(std::vector<ABCD::Digitizer*> &digitizers) {
        counter_updates += 1;

        lua_pushinteger(Lua, counter_updates);
        lua_setglobal(Lua, "counter_updates");

        absp_logger_console->info("LuaManager::update_digitizers() Number of updates: {}", counter_updates);

        absp_logger_console->info("LuaManager::update_digitizers() Number of digitizers: {}", digitizers.size());

        lua_getglobal(Lua, "digitizer_instances");

        for (auto &digitizer: digitizers) {
            absp_logger_console->info("LuaManager::update_digitizers() Found digitizer: {}", digitizer->GetModel());

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
                absp_logger_console->warn("LuaManager::update_digitizers() Using the generic Digitizer interface for the unexpected digitizer: {}; name: {}", digitizer->GetModel(), digitizer->GetName());

                SWIG_NewPointerObj(Lua, digitizer, SWIG_ABCD_Digitizer_t, 0);

                lua_setfield(Lua, -2, digitizer->GetName().c_str());
            }
        }

        // It is important to pop the digitizer_instances from the stack,
        // otherwise it would pile-up in it multiple times.
        lua_pop(Lua, 1);

        if (absp_logger_console->should_log(spdlog::level::debug))
        {
            run_string("abcd_log(\"LUA INTERPRETER DEBUG OUTPUT: LuaManager::update_digitizers() Finished\");");

            dump_stack();
        }
    }

    void run_string(std::string source) {
        counter_runs += 1;

        lua_pushinteger(Lua, counter_runs);
        lua_setglobal(Lua, "counter_runs");

        absp_logger_console->info("LuaManager::run_string() Number of runs: {}", counter_runs);

        const int result = luaL_dostring(Lua, source.c_str());

        if (result != LUA_OK) { // there was an error
            // Get the error message from the Lua stack
            const std::string error_message = lua_tostring(Lua, -1);

            switch (result) {
                case LUA_ERRSYNTAX:
                    absp_logger_error->error("LuaManager::run_string() Syntax error: {}", error_message);
                    break;
                case LUA_ERRMEM:
                    absp_logger_error->error("LuaManager::run_string() Memory allocation error: {}", error_message);
                    break;
                case LUA_ERRFILE:
                    absp_logger_error->error("LuaManager::run_string() File error: {}", error_message);
                    break;
                case LUA_ERRRUN:
                    absp_logger_error->error("LuaManager::run_string() Runtime error: {}", error_message);
                    break;
                case LUA_ERRERR:
                    absp_logger_error->error("LuaManager::run_string() Error handler error: {}", error_message);
                    break;
                default:
                    absp_logger_error->error("LuaManager::run_string() Unknown error: {}", error_message);
                    break;
            }

            // Pop the error message from the Lua stack
            lua_pop(Lua, 1);
        }
    }

    void run_script(unsigned int state_ID,
                    std::string state_description,
                    std::string when,
                    std::string source)
    {
        absp_logger_console->info("LuaManager::run_script() Running script; Current state ID: {}; Current state description: {}; When: {}", state_ID, state_description, when);

        lua_getglobal(Lua, "current_state");

        lua_pushinteger(Lua, state_ID);
        lua_setfield(Lua, -2, "ID");

        lua_pushstring(Lua, state_description.c_str());
        lua_setfield(Lua, -2, "description");

        lua_pushstring(Lua, when.c_str());
        lua_setfield(Lua, -2, "when");

        lua_pop(Lua, 1);

        run_string(source);

        if (absp_logger_console->should_log(spdlog::level::debug))
        {
            run_string("abcd_log(\"LUA INTERPRETER DEBUG OUTPUT: LuaManager::run_script() Finished\");");

            dump_stack();
        }
    }

    inline static int abcd_log(lua_State *L) {
        // Get the number of arguments
        const int n = lua_gettop(L);
        std::string output;

        for (int index = 1; index <= n; index += 1) {
            if (lua_isstring(L, index))
            {
                const char* argument = lua_tostring(L, index);

                output += argument;
                output += " ";

            } else if (lua_isnumber(L, index)) {
                const lua_Number argument = lua_tonumber(L, index);

                output += std::to_string(argument);
                output += " ";
            } else {
                lua_pushliteral(L, "the argument of the abcd_log() function must be a string or a number");
                lua_error(L);

                return 0;
            }
        }

        absp_logger_console->info(output);

        return 0;
    }

    inline static int sleep(lua_State *L) {
        // Get the number of arguments
        const int n = lua_gettop(L);

        if (n != 1) {
            lua_pushliteral(L, "the sleep() function expects one argument");
            lua_error(L);

            return 0;
        }

        if (!lua_isnumber(L, 1))
        {
            lua_pushliteral(L, "the argument of the sleep() function must be a number");
            lua_error(L);

            return 0;
        }

        const lua_Number milliseconds = lua_tonumber(L, 1);

        if (milliseconds < 0) {
            lua_pushliteral(L, "the argument of the sleep() function must be a non-negative");
            lua_error(L);

            return 0;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(milliseconds)));

        return 0;
    }
};

#endif
