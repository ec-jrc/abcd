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

function generate_tofcalc_reconfigure_arguments() {
    const channel_ids = $(".channel_ids").map(function (index, element) {
        return parseInt(this.value);
    }).get();

    const channels = channel_ids.map(function (id) {
        return {"bins_ToF": parseInt($("#ToF_bins_" + id).val()),
                "min_ToF": parseInt($("#ToF_min_" + id).val()),
                "max_ToF": parseInt($("#ToF_max_" + id).val()),
                "bins_E": parseInt($("#E_bins_" + id).val()),
                "min_E": parseInt($("#E_min_" + id).val()),
                "max_E": parseInt($("#E_max_" + id).val()),
                "id": id,
                "reference": $("#ToF_reference_" + id).is(':checked')
               };
    });

    const kwargs = {"channels": channels};

    console.log(JSON.stringify(kwargs));

    return kwargs;
}

function generate_tofcalc_reset_channel_arguments() {
    const selected_channel = parseInt($("#channel_select").val());

    const kwargs = {"channel": selected_channel,
                    "type": "all"};

    console.log(JSON.stringify(kwargs));

    return kwargs;
}
