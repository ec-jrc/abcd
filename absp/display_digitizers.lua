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
