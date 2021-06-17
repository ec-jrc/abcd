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

var abcd_config_editor;
var last_abcd_config = null;

var last_timestamps = {"abcd": null,
                       "hijk": null,
                       "lmno": null,
                       "pqrs": null
                      };
var interval_id = null;
var update_charts = null;

function socket_io_disconnection(error) {
    console.log("Server disconnected, error: " + error);

    window.clearInterval(interval_id);

    $('#efg_connection_status').removeClass();
    $('#efg_connection_status').addClass("bad_status").text("Connection lost");

    $('#abcd_connection_status').removeClass().addClass("unknown_status");
    $('#abcd_connection_status').text("No connection");
    $('#hijk_connection_status').removeClass().addClass("unknown_status");
    $('#hijk_connection_status').text("No connection");
    $('#lmno_connection_status').removeClass().addClass("unknown_status");
    $('#lmno_connection_status').text("No connection");
    $('#pqrs_connection_status').removeClass().addClass("unknown_status");
    $('#pqrs_connection_status').text("No connection");
}

function socket_io_connection(socket) {
    return function() {
        console.log("Connected to server");

        interval_id = window.setInterval(function () {
            display_connection("abcd");
            display_connection("hijk");
            display_connection("lmno");
            display_connection("pqrs");
        }, 500);

        $('#efg_connection_status').removeClass();
        $('#efg_connection_status').addClass("good_status").text("Connection OK");

        socket.on('event_abcd', generate_update_events("abcd"));
        socket.on('event_efg', generate_update_events("efg"));
        socket.on('event_hijk', generate_update_events("hijk"));
        socket.on('event_lmno', generate_update_events("lmno"));
        socket.on('event_pqrs', generate_update_events("pqrs"));

        socket.on('status_abcd', update_abcd_status);
        socket.on('status_efg', update_efg_status);
        socket.on('status_hijk', update_hijk_status);
        socket.on('status_lmno', update_lmno_status);
        socket.on('status_pqrs', update_pqrs_status);

        socket.on('data_wafi', generate_update_waveform());

        update_charts = generate_update_charts("qlong");
        socket.on('data_pqrs', update_charts.update);

        socket.on('disconnect', socket_io_disconnection);
    }
}

var page_loaded = (function() {
    var socket;

    return function() {
        socket = io();

        socket.on("connect", socket_io_connection(socket));

        $("#controls").tabs();
        //$("#accordion_abcd").accordion({heightStyle: "content"});
        //$("#accordion_hijk").accordion({heightStyle: "content"});
        //$("#accordion_lmno").accordion({heightStyle: "content"});

        abcd_config_editor = ace.edit("online_editor");
        abcd_config_editor.setTheme("ace/theme/github");
        abcd_config_editor.setShowPrintMargin(false);
        abcd_config_editor.setOptions({"fontFamily": '"Fira Mono"'});

        abcd_config_editor.getSession().setMode("ace/mode/json");
        abcd_config_editor.getSession().setTabSize(4);
        abcd_config_editor.getSession().setUseSoftTabs(true);

        abcd_config_editor.container.style.lineHeight = 2;
        abcd_config_editor.renderer.updateFontSize();
        abcd_config_editor.resize();

        $("#abcd_start").click(generate_send_command(socket, 'abcd', 'start'));
        $("#abcd_stop").click(generate_send_command(socket, 'abcd', 'stop'));
        $("#lmno_start").click(generate_send_command(socket, 'lmno', 'start', generate_lmno_arguments));
        $("#lmno_stop").click(generate_send_command(socket, 'lmno', 'stop'));
        $("#hijk_reconfigure").click(generate_send_command(socket, 'hijk', 'reconfigure', generate_hijk_arguments));
        $("#pqrs_reconfigure").click(generate_send_command(socket, 'pqrs', 'reconfigure', generate_pqrs_reconfigure_arguments));
        $("#pqrs_reset_channel").click(generate_send_command(socket, 'pqrs', 'reset', generate_pqrs_reset_channel_arguments));
        $("#pqrs_reset_all").click(generate_send_command(socket, 'pqrs', 'reset', {"channel": "all", "type": "all"}));
        $("#abcd_reconfigure").click(generate_send_command(socket, 'abcd', 'reconfigure', generate_abcd_reconfigure_arguments));
        $("#abcd_get_configuration").click(function () {
            if (_.isNil(last_abcd_config)) {
                abcd_config_editor.getSession().setValue(JSON.stringify({"error": "Unable to load ABCD config"}, null, 4));
                abcd_config_editor.gotoLine(0);
            } else {
                abcd_config_editor.getSession().setValue(JSON.stringify(last_abcd_config, null, 4));
                abcd_config_editor.gotoLine(0);
            }
        });

        $("#spectrum_yscale").change(function () {
            // The user requested a y scale change, so we shall remove the listener
            socket.removeListener('data_pqrs', update_charts.update);
            update_charts.chart.destroy();

            update_charts = generate_update_charts("qlong");
            socket.on('data_pqrs', update_charts.update);
        });

    }
})();

$(document).ready(page_loaded)
