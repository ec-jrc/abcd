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

print("    Using digitizer: " .. digitizer_instances["SPD-07721"]:GetName())
print("    Using digitizer: " .. digitizer_instances["SPD-06799"]:GetName())

print("Arming the timestamp reset using the trig port as input")
-- Possible mode values: "first" or "all"
-- WARNING: with "all" the board does not seem to trigger
mode = "first"
-- Possible source values: "software", "sync_port" or "trig_port"
source = "trig_port"

digitizer_instances["SPD-07721"]:TimestampResetArm(mode, source)
digitizer_instances["SPD-06799"]:TimestampResetArm(mode, source)

print("Setting the trig port as output")
-- Refer to the digitizer documentation for these values
port = 3
direction = 1
mask = 0

digitizer_instances["SPD-07721"]:GPIOSetDirection(port, direction, mask)

print("Generating a pulse from the sync port")
-- The width is in microseconds, a zero is equivalent to the minimum possible width
port = 3
mask = 0
width = 0

digitizer_instances["SPD-07721"]:GPIOPulse(port, mask, width)

print("Disarming the timestamp reset")

digitizer_instances["SPD-07721"]:TimestampResetDisarm()
digitizer_instances["SPD-06799"]:TimestampResetDisarm()
