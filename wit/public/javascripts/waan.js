// (C) Copyright 2016,2021 European Union, Cristiano Lino Fontana
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

function page_loaded() {
    const utf8decoder = new TextDecoder("utf8");

    var connection_checker = new ConnectionChecker();

    var socket_io = io();

    const module_name = String($('input#module_name').val());
    
    console.log("Module name: " + module_name);

    var old_status = {"timestamp": "###"};
    var last_waan_config = null;

    var waan_config_editor = ace.edit("online_editor");
    waan_config_editor.setTheme("ace/theme/github");
    waan_config_editor.setShowPrintMargin(false);
    waan_config_editor.setOptions({"fontFamily": '"Fira Mono"'});

    waan_config_editor.getSession().setMode("ace/mode/json");
    waan_config_editor.getSession().setTabSize(4);
    waan_config_editor.getSession().setUseSoftTabs(true);

    waan_config_editor.container.style.lineHeight = 2;
    waan_config_editor.renderer.updateFontSize();
    waan_config_editor.resize();

    function on_status(message) {
        const decoded_string = utf8decoder.decode(message);
        const new_status = JSON.parse(decoded_string);

        connection_checker.beat();

        if (new_status["timestamp"] !== old_status["timestamp"])
        {
            old_status = new_status;

            if (new_status.hasOwnProperty("config")) {
                last_waan_config = new_status["config"];
            }

            const new_channels_statuses = new_status["statuses"];

            let rates_list = $("<ul>");

            new_channels_statuses.forEach(function (channel_status) {
                const channel = channel_status["id"];
                const rate = channel_status["rate"];

                rates_list.append($("<li>", {text: "Ch " + channel + ": " + rate.toFixed(2)}));
            });

            $("#channels_rates").empty().append(rates_list);

            const new_channels_active = new_status["active_channels"];

            let active_list = $("<ul>");

            new_channels_active.forEach(function (channel) {
                active_list.append($("<li>", {text: "Ch " + channel}).addClass("good_status"));
            });

            $("#active_channels").empty().append(active_list);

            const new_channels_disabled = new_status["disabled_channels"];

            let disabled_list = $("<ul>");

            new_channels_disabled.forEach(function (channel) {
                disabled_list.append($("<li>", {text: "Ch " + channel}).addClass("bad_status"));
            });

            $("#disabled_channels").empty().append(disabled_list);
        }
    }

    function waan_arguments_config() {
        try {
            const new_text_config = waan_config_editor.getSession().getValue();
            const new_config = JSON.parse(new_text_config);
            const kwargs = {"config": new_config};

            return kwargs;

        } catch (error) {
            console.log("Error: " + error);

            return null;
        }
    }

    function waan_get_config() {
        if (_.isNil(last_waan_config)) {
            waan_config_editor.getSession().setValue(JSON.stringify({"error": "Unable to load waan config"}, null, 4));
            waan_config_editor.gotoLine(0);
        } else {
            waan_config_editor.getSession().setValue(JSON.stringify(last_waan_config, null, 4));
            waan_config_editor.gotoLine(0);
        }
    }

    function waan_download_config() {
        const new_text_config = waan_config_editor.getSession().getValue();
        create_and_download_file(new_text_config, "waan_config.json", "txt");
    }

    socket_io.on("connect", socket_io_connection(socket_io, module_name, on_status, update_events_log(), null));

    window.setInterval(function () {
        connection_checker.display();
    }, 1000);

    $("#button_config_send").on("click", send_command(socket_io, 'reconfigure', waan_arguments_config));
    $("#button_config_get").on("click", waan_get_config);
    $("#button_config_download").on("click", waan_download_config);
}

$(page_loaded);
