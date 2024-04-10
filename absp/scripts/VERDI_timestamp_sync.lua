-- WARNING: This searches for a file called json.lua in the current directory
--          where absp is being run or in one of the standard directories of Lua
json = require "json"

print("Current state:")
print("    ID: " .. current_state["ID"])
print("    when: " .. current_state["when"])
print("    description: " .. current_state["description"])

-- Indexes of the TRIG and SYNC pins, normally they should be 0 for the ADQAPI,
-- but in Lua indexes start from 1, so increasing them...
local TRIG_PIN = 1
local SYNC_PIN = 1

local digitizer_names = {"SPD-11321", "SPD-11322", "SPD-11323", "SPD-11324", "SPD-11325"}
local daisy_chain_positions = {
    ["SPD-11321"] = 0,
    ["SPD-11322"] = 1,
    ["SPD-11323"] = 2,
    ["SPD-11324"] = 3,
    ["SPD-11325"] = 4,
}
local first_device = "SPD-11321"
local roles = {
    ["SPD-11321"] = "Si_det",
    ["SPD-11322"] = "Si_det",
    ["SPD-11323"] = "Si_det",
    ["SPD-11324"] = "MCP",
    ["SPD-11325"] = "MCP",
}
local USER_LOGIC_MODE_FORWARD_TO_SYNC = 1
local USER_LOGIC_MODE_DAISY_CHAIN_ONLY = 0

local USER_LOGIC_PULSE_LENGTH = 100

for index, name in ipairs(digitizer_names) do
    print("    Using digitizer: " .. digitizer_instances[name]:GetName() .. " at position: " .. daisy_chain_positions[name])
end

print("Setting up the daisy chain for timestamp synchronization...")

for index, name in ipairs(digitizer_names) do
    local digitizer_instance = digitizer_instances[name]
    local daisy_chain_position = daisy_chain_positions[name]

    print("Configuring digitizer " .. name .. " (index: " .. index .. ") at position: " .. daisy_chain_position)

    print("    Shutting off sync output from user logic to not mess up with the timestamp synchronization...")
    digitizer_instance:CustomFirmwareEnable(false)

    -- WARNING: This is probably a bad idea, we do not trust yet the SWIG + json.lua interface
    --          We better do it piece by piece
    --print("    Reading parameters: ADQ_PARAMETER_ID_TOP (id: " .. ADQAPI.ADQ_PARAMETER_ID_TOP .. ")")
    --local parameters_top = json.decode(digitizer_instance:GetParametersString(ADQAPI.ADQ_PARAMETER_ID_TOP))

    print("    Reading parameters: ADQ_PARAMETER_ID_DAISY_CHAIN (id: " .. ADQAPI.ADQ_PARAMETER_ID_DAISY_CHAIN .. ")")
    local parameters_daisy_chain = json.decode(digitizer_instance:GetParametersString(ADQAPI.ADQ_PARAMETER_ID_DAISY_CHAIN))
    print("    Reading parameters: ADQ_PARAMETER_ID_TIMESTAMP_SYNCHRONIZATION (id: " .. ADQAPI.ADQ_PARAMETER_ID_TIMESTAMP_SYNCHRONIZATION .. ")")
    local parameters_timestamp_synchronization = json.decode(digitizer_instance:GetParametersString(ADQAPI.ADQ_PARAMETER_ID_TIMESTAMP_SYNCHRONIZATION))

    print("    Reading parameters: ADQ_PARAMETER_ID_EVENT_SOURCE_TRIG (id: " .. ADQAPI.ADQ_PARAMETER_ID_EVENT_SOURCE_TRIG .. ")")
    local parameters_event_source_trig = json.decode(digitizer_instance:GetParametersString(ADQAPI.ADQ_PARAMETER_ID_EVENT_SOURCE_TRIG))

    print("    Reading parameters: ADQ_PARAMETER_ID_PORT_TRIG (id: " .. ADQAPI.ADQ_PARAMETER_ID_PORT_TRIG .. ")")
    local parameters_port_trig = json.decode(digitizer_instance:GetParametersString(ADQAPI.ADQ_PARAMETER_ID_PORT_TRIG))
    print("    Reading parameters: ADQ_PARAMETER_ID_PORT_SYNC (id: " .. ADQAPI.ADQ_PARAMETER_ID_PORT_SYNC .. ")")
    local parameters_port_sync = json.decode(digitizer_instance:GetParametersString(ADQAPI.ADQ_PARAMETER_ID_PORT_SYNC))

    print("    Reading parameters: ADQ_PARAMETER_ID_CLOCK_SYSTEM (id: " .. ADQAPI.ADQ_PARAMETER_ID_CLOCK_SYSTEM .. ")")
    local parameters_clock_system = json.decode(digitizer_instance:GetParametersString(ADQAPI.ADQ_PARAMETER_ID_CLOCK_SYSTEM))

    if daisy_chain_position == 0 then
        print("    " .. name .. " is the first digitizer in the chain, setting its event source software parameters...")
        
        -- WARNING:
        -- Due to a bug in the SP Devices drivers, these parameters are not
        -- available in the global parameters when read as a JSON string, but it
        -- seems that it is writable anyways through the JSON interface
        
        local parameters_event_source_software = {
            ["id"] = "ADQ_PARAMETER_ID_EVENT_SOURCE_SOFTWARE",
            ["reference_clock_synchronization_enabled"] = 1,
            ["reference_clock_synchronization_edge"] = "ADQ_EDGE_RISING",
        }
        
        print("    Sending the event source software parameters: " .. json.encode(parameters_event_source_software))
        
        digitizer_instance:SetParametersString(json.encode(parameters_event_source_software))

        print("    Parameters: Daisy chain source: " .. parameters_daisy_chain.source .. " setting to: ADQ_EVENT_SOURCE_SOFTWARE")
        parameters_daisy_chain.source = "ADQ_EVENT_SOURCE_SOFTWARE"

    else

        print("    " .. name .. " is not the first digitizer in the chain...")
        print("    Parameters: Reference clock synchronization: " .. parameters_event_source_trig.pin[TRIG_PIN].reference_clock_synchronization_enabled)
        parameters_event_source_trig.pin[TRIG_PIN].reference_clock_synchronization_enabled = 1

        print("    Parameters: Daisy chain source: " .. parameters_daisy_chain.source .. " setting to: " .. "ADQ_EVENT_SOURCE_TRIG")
        parameters_daisy_chain.source = "ADQ_EVENT_SOURCE_TRIG"
    end

    print("    Set up common daisy chain parameters...")

    print("    Parameters: Daisy chain position: " .. parameters_daisy_chain.position .. " setting to: " .. daisy_chain_position)
    parameters_daisy_chain.position = daisy_chain_position

    print("    Parameters: Daisy chain arm: " .. parameters_daisy_chain.arm .. " setting to: " .. "ADQ_ARM_IMMEDIATELY")
    parameters_daisy_chain.arm = "ADQ_ARM_IMMEDIATELY"

    print("    Parameters: Daisy chain resynchronization enabled: " .. parameters_daisy_chain.resynchronization_enabled)
    parameters_daisy_chain.resynchronization_enabled = 1

    print("    Parameters: Port SYNC pin 0 function: " .. parameters_port_sync.pin[SYNC_PIN]["function"] .. " setting to: " ..  "ADQ_FUNCTION_DAISY_CHAIN")
    parameters_port_sync.pin[SYNC_PIN]["function"] = "ADQ_FUNCTION_DAISY_CHAIN"

    print("    Parameters: Port SYNC pin 0 direction: " .. parameters_port_sync.pin[SYNC_PIN].direction .. " setting to: " .. "ADQ_DIRECTION_OUT")
    parameters_port_sync.pin[SYNC_PIN].direction = "ADQ_DIRECTION_OUT"

    print("    Set up timestamp sync parameters...")

    print("    Parameters: Timestamp sync source: " .. parameters_timestamp_synchronization.source .. " setting to: " .. parameters_daisy_chain.source)
    parameters_timestamp_synchronization.source = parameters_daisy_chain.source

    print("    Parameters: Timestamp sync mode: " .. parameters_timestamp_synchronization.mode .. " setting to: " ..  "ADQ_TIMESTAMP_SYNCHRONIZATION_MODE_FIRST")
    parameters_timestamp_synchronization.mode = "ADQ_TIMESTAMP_SYNCHRONIZATION_MODE_FIRST"

    print("    Parameters: Timestamp sync arm: " .. parameters_timestamp_synchronization.arm .. " setting to: " ..  "ADQ_ARM_IMMEDIATELY")
    parameters_timestamp_synchronization.arm = "ADQ_ARM_IMMEDIATELY"


    print("Calculating seed:")

    print("    Sampling frequency: " .. parameters_clock_system.sampling_frequency .. " Hz")
    print("    Reference frequency: " .. parameters_clock_system.reference_frequency .. " Hz")

    timestamp_reference_samples = parameters_clock_system.sampling_frequency / parameters_clock_system.reference_frequency

    print("    Timestamp reference samples: " .. timestamp_reference_samples)

    local seed = math.floor(16 * daisy_chain_position * timestamp_reference_samples)
    print("    Seed: " .. seed)

    -- WARNING: In the default parameters I see:
    -- "timestamp_synchronization":    {
    --         "id":   "ADQ_PARAMETER_ID_TIMESTAMP_SYNCHRONIZATION",
    --         "source":       "ADQ_EVENT_SOURCE_INVALID",
    --         "edge": "ADQ_EDGE_RISING",
    --         "mode": "ADQ_TIMESTAMP_SYNCHRONIZATION_MODE_DISABLE",
    --         "arm":  "ADQ_ARM_IMMEDIATELY",
    --         "seed": "0"
    -- }
    -- So the seed seems like a string, maybe it is beacause uint64_t have a problematic support by the JSON standard
 
    parameters_timestamp_synchronization.seed = tostring(seed)

    print("    Setting daisy chain parameters...")
    digitizer_instance:SetParametersString(json.encode(parameters_daisy_chain))
    print("    Setting timestamp synchronization parameters...")
    digitizer_instance:SetParametersString(json.encode(parameters_timestamp_synchronization))
    print("    Setting event source trig parameters...")
    digitizer_instance:SetParametersString(json.encode(parameters_event_source_trig))
    print("    Setting port trig parameters...")
    digitizer_instance:SetParametersString(json.encode(parameters_port_trig))
    print("    Setting port sync parameters...")
    digitizer_instance:SetParametersString(json.encode(parameters_port_sync))

    -- TODO: Check results of the functions
    --if result > 0 then
    --    print("    Setting the parameters for " .. name .. " was successful")
    --else
    --    print("    ERROR: Error in setting the parameters for " .. name)
    --end
end

print("Sending the timestamps synchronization pulse...")
digitizer_instances[first_device]:ForceSoftwareTrigger()

print("Verifying timestamp synchronization status...")
for index, name in ipairs(digitizer_names) do
    local digitizer_instance = digitizer_instances[name]

    print("    Reading status: ADQ_STATUS_ID_TIMESTAMP_SYNCHRONIZATION (id: " .. ADQAPI.ADQ_STATUS_ID_TIMESTAMP_SYNCHRONIZATION .. ")")
    local status = json.decode(digitizer_instance:GetStatusString(ADQAPI.ADQ_STATUS_ID_TIMESTAMP_SYNCHRONIZATION))

    print("    Counter: " .. status.counter)
end

-- TODO: We could verify parameters here

print("Setting up the ports function to normal operation...")

for index, name in ipairs(digitizer_names) do
    local digitizer_instance = digitizer_instances[name]
    local role = roles[name]

    print("Configuring digitizer " .. name .. " (index: " .. index .. ") with role: " .. role)

    print("    Reading parameters: ADQ_PARAMETER_ID_EVENT_SOURCE_TRIG (id: " .. ADQAPI.ADQ_PARAMETER_ID_EVENT_SOURCE_TRIG .. ")")
    local parameters_event_source_trig = json.decode(digitizer_instance:GetParametersString(ADQAPI.ADQ_PARAMETER_ID_EVENT_SOURCE_TRIG))

    print("    Reading parameters: ADQ_PARAMETER_ID_PORT_TRIG (id: " .. ADQAPI.ADQ_PARAMETER_ID_PORT_TRIG .. ")")
    local parameters_port_trig = json.decode(digitizer_instance:GetParametersString(ADQAPI.ADQ_PARAMETER_ID_PORT_TRIG))
    print("    Reading parameters: ADQ_PARAMETER_ID_PORT_SYNC (id: " .. ADQAPI.ADQ_PARAMETER_ID_PORT_SYNC .. ")")
    local parameters_port_sync = json.decode(digitizer_instance:GetParametersString(ADQAPI.ADQ_PARAMETER_ID_PORT_SYNC))

    print("    Configuring the ports...")
    parameters_event_source_trig.pin[TRIG_PIN].reference_clock_synchronization_enabled = 0

    parameters_port_sync.pin[SYNC_PIN]["function"] = "ADQ_FUNCTION_USER_LOGIC"
    parameters_port_sync.pin[SYNC_PIN].direction = "ADQ_DIRECTION_OUT"
    parameters_port_trig.pin[TRIG_PIN]["function"] = "ADQ_FUNCTION_INVALID"
    parameters_port_trig.pin[TRIG_PIN].direction = "ADQ_DIRECTION_IN"

    print("    Setting event source trig parameters...")
    digitizer_instance:SetParametersString(json.encode(parameters_event_source_trig))
    print("    Setting port trig parameters...")
    digitizer_instance:SetParametersString(json.encode(parameters_port_trig))
    print("    Setting port sync parameters...")
    digitizer_instance:SetParametersString(json.encode(parameters_port_sync))

    print("    Setting up the user logic...")
    if role == "Si_det" then
        digitizer_instance:CustomFirmwareSetMode(USER_LOGIC_MODE_FORWARD_TO_SYNC)
    else
        digitizer_instance:CustomFirmwareSetMode(USER_LOGIC_MODE_DAISY_CHAIN_ONLY)
    end

    digitizer_instance:CustomFirmwareSetPulseLength(USER_LOGIC_PULSE_LENGTH)

    digitizer_instance:CustomFirmwareEnable(true)
end
