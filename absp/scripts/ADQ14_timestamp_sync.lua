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
local MASTER_DIGITIZER_NAME = "SPD-07716"

-- These are the digitizers that need to be sync'ed, the script will verify if
-- they are present in the instances
local DIGITIZER_NAMES = { "SPD-07716", "SPD-04637", "SPD-04638", "SPD-04642", "SPD-04643", "SPD-04640", "SPD-04641", "SPD-09518", "SPD-05336", "SPD-05334"}

local detected_names = {}

print("Digitizers:")
for index, name in ipairs(DIGITIZER_NAMES) do
    print("Detecting digitizer: " .. name)

    if digitizer_instances[name] then
        print("    Detected digitizer: " .. digitizer_instances[name]:GetName())
	table.insert(detected_names, name)
    else
        print("    ERROR detecting digitizer")
    end
end

local mode = "first"

print("Arming the timestamp reset using the TRIG port as input")
local source = "trig_port"
--print("Arming the timestamp reset using the SYNC port as input")
--local source = "trig_port"

for index, name in ipairs(detected_names) do
    digitizer_instances[name]:TimestampResetArm(mode, source)
end

if digitizer_instances[MASTER_DIGITIZER_NAME] then
    local master_digitizer = digitizer_instances[MASTER_DIGITIZER_NAME]

    print("Setting the SYNC port of the master digitizer as output")
    -- Refer to the digitizer documentation for these values
    local port = 3
    -- Direction: 0 for input, 1 for output
    local direction = 1
    -- Only bits which are zero-valued in the mask will be changed
    local mask = 0

    master_digitizer:GPIOSetDirection(port, direction, mask)

    sleep(100)

    print("Generating a pulse from the SYNC port of the master digitizer")
    -- The width is in microseconds, a zero is equivalent to the minimum possible width
    local width = 1

    master_digitizer:GPIOPulse(port, mask, width)

    sleep(100)
else
    print("ERROR detecting master digitizer: " .. MASTER_DIGITIZER_NAME)
end

print("Disarming the timestamp reset")

for index, name in ipairs(detected_names) do
    digitizer_instances[name]:TimestampResetDisarm()
end
