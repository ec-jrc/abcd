--- This script shows an example of the Lua interface for absp.
-- It uses the functions declared in include/ADQ214.hpp 
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
local MASTER_DIGITIZER = "SPD-03864"

local digitizer_names = { "SPD-01370", "SPD-01372", "SPD-01373", "SPD-01806", "SPD-02023", "SPD-02850", "SPD-02851", "SPD-02853", "SPD-02854", "SPD-02855", "SPD-03862", "SPD-03863", "SPD-03864", "SPD-03868", "SPD-03871", "SPD-03873"}

print("Digitizers:")
print("    Master digitizer: " .. MASTER_DIGITIZER)

for index, name in ipairs(digitizer_names) do
    print("    Using digitizer: " .. digitizer_instances[name]:GetName())
end

print("Setting the GPIO port, pin 5, as output")
-- Refer to the digitizer documentation for these values
-- On the ADQ214 pin 5 (bit 4) has a high drive capacity with only 30 ohms output impedance.
-- Direction: 0 for input, 1 for output
local pins_directions = (1 << 4)
-- Only 5 GPIO pins are available on the ADQ214, so only the first 5 bits of the mask are used
-- Only bits which are zero-valued in the mask will be changed
local mask = 0xF

digitizer_instances[MASTER_DIGITIZER]:GPIOSetDirection(pins_directions, mask)

sleep(100)

for index, name in ipairs(digitizer_names) do
    print("Set the timestamp reset mode to \"pulse\" for digitizer: " .. name)
    digitizer_instances[name]:TimestampResetMode("pulse")
end

-- On the ADQ214 pin 5 (bit 4) has a high drive capacity with only 30 ohms output impedance.
print("Generating a pulse from the pin 5 of the GPIO port")
-- Only bits which are zero-valued in the mask will be changed
local mask = 0xF
-- The width is in microseconds, a zero is equivalent to the minimum possible width
local width = 1

digitizer_instances[MASTER_DIGITIZER]:GPIOPulse(width, mask)
