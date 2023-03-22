#! /bin/bash

# Examples of specific commands for the ADQ214

#~/abcd/bin/send_command.py specific -a '{"serial": "SPD-01373", "command": "timestamp_reset", "mode": "pulse"}'
#~/abcd/bin/send_command.py specific -a '{"serial": "SPD-03867", "command": "timestamp_reset", "mode": "pulse"}'
#~/abcd/bin/send_command.py specific -a '{"serial": "SPD-02020", "command": "timestamp_reset", "mode": "pulse"}'
#~/abcd/bin/send_command.py specific -a '{"serial": "SPD-01335", "command": "timestamp_reset", "mode": "pulse"}'
#~/abcd/bin/send_command.py specific -a '{"serial": "SPD-03875", "command": "timestamp_reset", "mode": "pulse"}'
#
#sleep 2
#
#~/abcd/bin/send_command.py specific -a '{"serial": "SPD-01373", "command": "GPIO_set_direction", "direction": "output"}'
#~/abcd/bin/send_command.py specific -a '{"serial": "SPD-01373", "command": "GPIO_pulse", "width": 0}'
#
#../bin/send_command.py specific -a '{"serial": "SPD-01373", "command": "GPIO_write", "value": 1}'
#../bin/send_command.py specific -a '{"serial": "SPD-01373", "command": "GPIO_read"}'
#../bin/send_command.py specific -a '{"serial": "SPD-01373", "command": "GPIO_write", "value": 0}'
#../bin/send_command.py specific -a '{"serial": "SPD-01373", "command": "GPIO_read"}'

# Examples of specific commands for the ADQ412

#~/abcd/bin/send_command.py specific -a '{"serial": "SPD-00858", "command": "timestamp_reset", "mode": "pulse"}'
#~/abcd/bin/send_command.py specific -a '{"serial": "SPD-00864", "command": "timestamp_reset", "mode": "pulse"}'
#
#sleep 2
#
#~/abcd/bin/send_command.py specific -a '{"serial": "SPD-01373", "command": "GPIO_set_direction", "direction": "output"}'
#~/abcd/bin/send_command.py specific -a '{"serial": "SPD-01373", "command": "GPIO_pulse", "width": 0}'
#
#../bin/send_command.py specific -a '{"serial": "SPD-01373", "command": "GPIO_write", "value": 1}'
#../bin/send_command.py specific -a '{"serial": "SPD-01373", "command": "GPIO_read"}'
#../bin/send_command.py specific -a '{"serial": "SPD-01373", "command": "GPIO_write", "value": 0}'
#../bin/send_command.py specific -a '{"serial": "SPD-01373", "command": "GPIO_read"}'

# Examples of specific commands for the ADQ14

# Using the trig port as an output
#~/abcd/bin/send_command.py specific -a '{"serial": "SPD-07721", "command": "GPIO_set_direction", "port": 2, "direction": 1, "mask": 0}'
#~/abcd/bin/send_command.py specific -a '{"serial": "SPD-07721", "command": "GPIO_pulse", "port": 2, "mask": 0, "width": 0}'
