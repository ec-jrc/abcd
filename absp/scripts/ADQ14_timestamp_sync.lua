--- This script shows an example of the Lua interface for absp.
-- It uses the functions declared in include/ADQ14_FWPD.hpp 
-- The functions used here are specific to that interface and would not work
-- with other models and firmwares.
-- To see what methods are supported by the other interfaces, see their header
-- files in include/
-- This timestamp synchronization could be run right after every configuration
-- of the digitizers, see the configuration ADQ14-FWPD.json for an example on
-- how to select the state.

print("Current state:")
print("    ID: " .. current_state["ID"])
print("    when: " .. current_state["when"])
print("    description: " .. current_state["description"])

-- The master digitizer emits the physical pulse that resets all the timestamps
local MASTER_DIGITIZER = "SPD-07722"

-- These digitizers are sync'ed with the SYNC port
local digitizer_names_sync = { }
-- These digitizers are sync'ed with the TRIG port
local digitizer_names_trig = { "SPD-10089", "SPD-05338", "SPD-05332", "SPD-05335", "SPD-07717", "SPD-07719", "SPD-07720", "SPD-07722" }

print("Digitizers:")
for index, name in ipairs(digitizer_names_sync) do
    print("    Using digitizer: " .. digitizer_instances[name]:GetName())
end
for index, name in ipairs(digitizer_names_trig) do
    print("    Using digitizer: " .. digitizer_instances[name]:GetName())
end

print("Arming the timestamp reset using the SYNC port as input")
-- Possible mode values: "first" or "all"
-- WARNING: with "all" the board does not seem to trigger
local mode = "first"
-- Possible source values: "software", "sync_port" or "trig_port"
local source = "sync_port"

for index, name in ipairs(digitizer_names_sync) do
    digitizer_instances[name]:TimestampResetArm(mode, source)
end

print("Arming the timestamp reset using the TRIG port as input")
mode = "first"
source = "trig_port"

for index, name in ipairs(digitizer_names_trig) do
    digitizer_instances[name]:TimestampResetArm(mode, source)
end

print("Setting the SYNC port as output")
-- Refer to the digitizer documentation for these values
local port = 3
-- Direction: 0 for input, 1 for output
local direction = 1
-- Only bits which are zero-valued in the mask will be changed
local mask = 0

digitizer_instances[MASTER_DIGITIZER]:GPIOSetDirection(port, direction, mask)

sleep(100)

print("Generating a pulse from the SYNC port")
port = 3
mask = 0
-- The width is in microseconds, a zero is equivalent to the minimum possible width
width = 1

digitizer_instances[MASTER_DIGITIZER]:GPIOPulse(port, mask, width)

sleep(100)

print("Disarming the timestamp reset")

--digitizer_instances["SPD-05333"]:TimestampResetDisarm()
digitizer_instances["SPD-10089"]:TimestampResetDisarm()
digitizer_instances["SPD-05338"]:TimestampResetDisarm()
digitizer_instances["SPD-05332"]:TimestampResetDisarm()
digitizer_instances["SPD-05335"]:TimestampResetDisarm()
digitizer_instances["SPD-07717"]:TimestampResetDisarm()
digitizer_instances["SPD-07719"]:TimestampResetDisarm()
digitizer_instances["SPD-07720"]:TimestampResetDisarm()
digitizer_instances["SPD-07722"]:TimestampResetDisarm()
for index, name in ipairs(digitizer_names_sync) do
    digitizer_instances[name]:TimestampResetDisarm()
end
for index, name in ipairs(digitizer_names_trig) do
    digitizer_instances[name]:TimestampResetDisarm()
end
