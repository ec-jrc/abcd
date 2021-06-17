// (C) Copyright 2016 Cristiano Lino Fontana
//
// This file is part of ABCD.
//
// ABCD is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// ABCD is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with ABCD.  If not, see <http://www.gnu.org/licenses/>.

'use strict';

function generate_send_command (socket, destination, command, kwargs_generator) {
    var msg_counter = 0;

    return function () {
        var kwargs = null;

        if (!_.isNil(kwargs_generator)) {
            if (_.isFunction(kwargs_generator)) {
                kwargs = kwargs_generator();
            } else {
                kwargs = kwargs_generator;
            }
        }

        var now = moment().format();

        var data = {"command": command,
                    "arguments": kwargs,
                    "msg_ID": msg_counter,
                    "timestamp": now
                   };

        socket.emit("command_" + destination, data);

        msg_counter += 1;
    }
}

function generate_abcd_reconfigure_arguments() {
    try {
        const new_text_config = abcd_config_editor.getSession().getValue();
        const new_config = JSON.parse(new_text_config);

        console.log(JSON.stringify(new_config));

        return {"config": new_config};

    } catch (error) {
        console.log("Error: " + error);
    }

    return null;
}

function generate_hijk_arguments() {
    var new_config = {};

    new_config["model_number"] = parseInt($("#model_number").val());
    new_config["connection_type"] = parseInt($("#connection_type").val());
    new_config["link_number"] = parseInt($("#link_number").val());
    new_config["CONET_node"] = parseInt($("#CONET_node").val());
    new_config["VME_address"] = parseInt($("#VME_address").val());
    new_config["channels"] = [];

    var channels = $("#hv_channels").val();

    for (var channel = 0; channel < channels; channel++) {
        var channel_config = {};

        channel_config["id"] = parseInt($("#channel_id_" + channel).val());
        channel_config["on"] = $("#channel_on_" + channel).prop("checked");
        channel_config["voltage"] = parseInt($("#voltage_" + channel).val());
        channel_config["max_current"] = parseInt($("#max_current_" + channel).val());
        channel_config["max_voltage"] = parseInt($("#max_voltage_" + channel).val());
        channel_config["ramp_up"] = parseInt($("#ramp_up_" + channel).val());
        channel_config["ramp_down"] = parseInt($("#ramp_down_" + channel).val());

        new_config["channels"].push(channel_config);
    }

    return {"config": new_config};
}

function generate_lmno_arguments() {
    var enable = {};
    enable["events"] = $("#events_file_enable").prop("checked");
    enable["waveforms"] = $("#waveforms_file_enable").prop("checked");
    enable["raw"] = $("#raw_file_enable").prop("checked");

    var kwargs = {"file_name": $("#lmno_file").val(), "enable": enable};

    console.log(JSON.stringify(kwargs));

    return kwargs;
}

function generate_pqrs_reconfigure_arguments() {
    const type = "qlong";

    var new_config = {};

    new_config["bins"] = parseInt($("#" + type + "_bins").val());
    new_config["min"] = parseInt($("#" + type + "_min").val());
    new_config["max"] = parseInt($("#" + type + "_max").val());

    const selected_channel = parseInt($("#channel_select").val());

    const kwargs = {"channel": selected_channel,
                    "type": type,
                    "config": new_config};

    console.log(JSON.stringify(kwargs));

    return kwargs;
}

function generate_pqrs_reset_channel_arguments() {
    const selected_channel = parseInt($("#channel_select").val());

    const kwargs = {"channel": selected_channel,
                    "type": "all"};

    console.log(JSON.stringify(kwargs));

    return kwargs;
}
