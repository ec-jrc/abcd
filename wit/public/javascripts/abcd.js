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
    var last_timestamp = null;
    var last_abcd_config = null;

    var abcd_config_editor = ace.edit("online_editor");
    abcd_config_editor.setTheme("ace/theme/github");
    abcd_config_editor.setShowPrintMargin(false);
    abcd_config_editor.setOptions({"fontFamily": '"Fira Mono"'});

    abcd_config_editor.getSession().setMode("ace/mode/json");
    abcd_config_editor.getSession().setTabSize(4);
    abcd_config_editor.getSession().setUseSoftTabs(true);

    abcd_config_editor.container.style.lineHeight = 2;
    abcd_config_editor.renderer.updateFontSize();
    abcd_config_editor.resize();

    function on_status(message) {
        const decoded_string = utf8decoder.decode(message);
        const new_status = JSON.parse(decoded_string);

        connection_checker.beat();

        if (new_status["timestamp"] !== old_status["timestamp"])
        {
            last_timestamp = new_status["timestamp"];

            old_status = new_status;

            if (new_status.hasOwnProperty("config")) {
                last_abcd_config = new_status["config"];
            }

            var status_list = $("<ul>");

            //if (new_status.hasOwnProperty("config")) {
            //    if (new_status["config"]["abcd"]["id"]) {
            //        $("<li>").text('Module ID: ' + new_status["config"]["abcd"]["id"]).appendTo(status_list);
            //    }
            //}

            if (new_status.hasOwnProperty("digitizer")) {
                $("<li>").text('Digitizer active: ' + new_status["digitizer"]["active"]).addClass("good_status").appendTo(status_list);
            } else {
                $("<li>").text('Digitizer active: Status Error').addClass("bad_status").appendTo(status_list);
            }

            if (new_status.hasOwnProperty("acquisition")) {
                var running = new_status["acquisition"]["running"];

                if (running) {
                    $("<li>").text('Acquisition running: ' + running).addClass("good_status").appendTo(status_list);

                    var run_time = dayjs.duration(new_status["acquisition"]["runtime"], "seconds");
                    var rates = new_status["acquisition"]["rates"];
                    var ICR_rates = new_status["acquisition"]["ICR_rates"];
                    var counts = new_status["acquisition"]["counts"];
                    var ICR_counts = new_status["acquisition"]["ICR_counts"];

                    $("<li>").text("Run time: " + humanizeDuration(run_time.asMilliseconds()) + " (" + run_time.asSeconds() + " s)").appendTo(status_list);

                    var rates_item = $("<li>").text("Rates:");
                    var rates_list = $("<ol>")

                    for (var i = 0; i < rates.length; i++)
                    {
                        $("<li>").text("Ch " + i + ": " + rates[i].toFixed(2)).appendTo(rates_list);
                    }

                    rates_item.append(rates_list);
                    status_list.append(rates_item);

                    var ICR_rates_item = $("<li>").text("ICR Rates:");
                    var ICR_rates_list = $("<ol>")

                    for (var i = 0; i < ICR_rates.length; i++)
                    {
                        $("<li>").text("Ch " + i + ": " + ICR_rates[i].toFixed(2)).appendTo(ICR_rates_list);
                    }

                    ICR_rates_item.append(ICR_rates_list);
                    status_list.append(ICR_rates_item);

                    var avg_rates_item = $("<li>").text("Average rates:");
                    var avg_rates_list = $("<ol>")

                    for (var i = 0; i < rates.length; i++)
                    {
                        $("<li>").text("Ch " + i + ": " + (counts[i] / run_time.asSeconds()).toFixed(2)).appendTo(avg_rates_list);
                    }

                    avg_rates_item.append(avg_rates_list);
                    status_list.append(avg_rates_item);

                    var counts_item = $("<li>").text("Total counts:");
                    var counts_list = $("<ol>")

                    for (var i = 0; i < counts.length; i++)
                    {
                        $("<li>").text("Ch " + i + ": " + (counts[i]|0)).appendTo(counts_list);
                    }

                    counts_item.append(counts_list);
                    status_list.append(counts_item);

                    var ICR_counts_item = $("<li>").text("Total ICR counts:");
                    var ICR_counts_list = $("<ol>")

                    for (var i = 0; i < ICR_counts.length; i++)
                    {
                        $("<li>").text("Ch " + i + ": " + (ICR_counts[i]|0)).appendTo(ICR_counts_list);
                    }

                    ICR_counts_item.append(ICR_counts_list);
                    status_list.append(ICR_counts_item);
                } else {
                    $("<li>").text('Acquisition running: ' + running).addClass("bad_status").appendTo(status_list);
                }
            } else {
                $("<li>").text('Acquisition running: Status Error').addClass("bad_status").appendTo(status_list);
            }

            $("#module_status").html(status_list);
        }
    }

    function start_timer() {
        const delay_min = parseFloat($("#acquisition_time").val());
        const delay_ms = delay_min * 60 * 1000;

        send_command(socket_io, 'start')();
    
        console.log("Starting an acquisition of " + delay_min + " min (" + delay_ms + " ms)");

        setTimeout(function () {
            console.log("End of the acquisition of " + delay_min + " min");
            send_command(socket_io, 'stop')();
        }, delay_ms);
    }

    function abcd_arguments_config() {
        try {
            const new_text_config = abcd_config_editor.getSession().getValue();
            const new_config = JSON.parse(new_text_config);
            const kwargs = {"config": new_config};

            return kwargs;

        } catch (error) {
            console.log("Error: " + error);

            return null;
        }
    }

    function abcd_get_config() {
        if (_.isNil(last_abcd_config)) {
            abcd_config_editor.getSession().setValue(JSON.stringify({"error": "Unable to load ABCD config"}, null, 4));
            abcd_config_editor.gotoLine(0);
        } else {
            abcd_config_editor.getSession().setValue(JSON.stringify(last_abcd_config, null, 4));
            abcd_config_editor.gotoLine(0);
        }
    }

    function abcd_download_config() {
        const new_text_config = abcd_config_editor.getSession().getValue();
        create_and_download_file(new_text_config, "abcd_config.json", "txt");
    }

    socket_io.on("connect", socket_io_connection(socket_io, module_name, on_status, update_events_log(), null));

    window.setInterval(function () {
        connection_checker.display();
    }, 1000);

    $("#button_start").on("click", send_command(socket_io, 'start'));
    $("#button_stop").on("click", send_command(socket_io, 'stop'));
    $("#button_starttimer").on("click", start_timer);
    $("#button_config_send").on("click", send_command(socket_io, 'reconfigure', abcd_arguments_config));
    $("#button_config_get").on("click", abcd_get_config);
    $("#button_config_download").on("click", abcd_download_config);
}

$(page_loaded);