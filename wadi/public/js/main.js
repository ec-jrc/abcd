// (C) Copyright 2016,2020 Cristiano Lino Fontana
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
    var module = "wadi";

    var update_plot = generate_update_plot();

    function socket_io_disconnection(error) {
        console.log("Server disconnected, error: " + error);

        window.clearInterval(interval_id);

        $('#gui_connection_status').removeClass();
        $('#gui_connection_status').addClass("bad_status").text("Connection lost");

        $('#wadi_connection_status').removeClass().addClass("unknown_status");
        $('#wadi_connection_status').text("No connection");
    }

    function socket_io_connection(socket) {
        return function() {
            console.log("Connected to server");
            interval_id = window.setInterval(function() {display_connection("wadi");}, 500);

            $('#gui_connection_status').removeClass();
            $('#gui_connection_status').addClass("good_status").text("Connection OK");

            socket.on('event_gui', generate_update_events("gui"));

            socket.on('data_wadi', update_plot);

            socket.on('disconnect', socket_io_disconnection);
        }
    }

    var socket;

    return function() {
        socket = io();

        socket.on("connect", socket_io_connection(socket));

        $("#channel_select").change(function () {
            console.log("Changed channel");
            next_update_plot = moment().add(-10, "seconds");
            update_plot();
        });

    }
})();

$(document).ready(page_loaded)
