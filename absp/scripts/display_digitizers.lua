--- This script shows an example of the Lua interface for absp.
-- Information about the state of absp is stored in the global variable:
--
--   current_state
--
-- That is a table with the keys:
-- * "ID": which shows the numerical ID of the current state;
-- * "when": which informs when the script is being run, whether "pre" or "post"
-- * "description": which holds a string with a human-readable description
--
-- The digitizers may be accessed through the global variable:
--
--   digitizer_instances
--
-- That is a table with the keys defined as the digitizers names (i.e. their
-- serial numbers). This example shows the general methods of the Digitizer
-- class, but each digitizer might have more functions. Refer to the interfaces
-- in the include/ folder. The GetModel() method programmatically informs which
-- interface of the include/ folder is used.
--
-- See the example configuration of the ADQ14_FWPD interface to see how to
-- enable a Lua script.

print("Current state:")
print("    ID: " .. current_state["ID"])
print("    when: " .. current_state["when"])
print("    description: " .. current_state["description"])

print("Digitizer instances:")
for k, v in pairs(digitizer_instances) do
  print("Table key: " .. k)
  print("    Model: " .. v:GetModel())
  print("    Name: " .. v:GetName())
  print("    Number of channels: " .. v:GetChannelsNumber())
  print("    SWIG Data type: '" .. swig_type(v) .. "'")
end
