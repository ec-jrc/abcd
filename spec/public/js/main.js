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

var last_timestamp = null;
var next_update_plot = moment();

var page_loaded = (function() {
    var interval_id = null;
    var module = "spec";

    var update_plot = generate_update_plot();

    function socket_io_disconnection(error) {
        console.log("Server disconnected, error: " + error);

        window.clearInterval(interval_id);

        $('#gui_connection_status').removeClass();
        $('#gui_connection_status').addClass("bad_status").text("Connection lost");

        $('#spec_connection_status').removeClass().addClass("unknown_status");
        $('#spec_connection_status').text("No connection");
    }

    function socket_io_connection(socket) {
        return function() {
            console.log("Connected to server");
            interval_id = window.setInterval(function() {display_connection("spec");}, 500);

            $('#gui_connection_status').removeClass();
            $('#gui_connection_status').addClass("good_status").text("Connection OK");

            socket.on('event_gui', generate_update_events("gui"));
            socket.on('event_spec', generate_update_events("spec"));

            socket.on('status_gui', update_gui_status);
            socket.on('status_spec', generate_update_spec_status());
            socket.on('data_spec', update_plot);

            socket.on('disconnect', socket_io_disconnection);
        }
    }

    var socket;

    return function() {
        socket = io();

        socket.on("connect", socket_io_connection(socket));

        $("#spec_reconfigure").click(generate_send_command(socket, 'spec', 'reconfigure', generate_spec_reconfigure_arguments));
        $("#spec_reset_channel").click(generate_send_command(socket, 'spec', 'reset', generate_spec_reset_channel_arguments));
        $("#spec_reset_all").click(generate_send_command(socket, 'spec', 'reset', {"channel": "all", "type": "all"}));
        $("#spectrum_yscale").change(function () {
            console.log("Changed y scale");
            next_update_plot = moment().add(-10, "seconds");
            update_plot();
        });
        $("#PSD_colors_scale").change(function () {
            console.log("Changed colors scale");
            next_update_plot = moment().add(-10, "seconds");
            update_plot();
        });
        $("#channel_select").change(function () {
            console.log("Changed channel");
            next_update_plot = moment().add(-10, "seconds");
            update_plot();
        });

    }
})();

$(document).ready(page_loaded)
