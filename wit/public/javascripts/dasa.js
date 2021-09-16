// (C) Copyright 2016,2020,2021 European Union, Cristiano Lino Fontana
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

"use strict";

function page_loaded() {
    const utf8decoder = new TextDecoder("utf8");

    var connection_checker = new ConnectionChecker();

    var socket_io = io();

    const module_name = String($('input#module_name').val());
    
    console.log("Module name: " + module_name);

    function on_status(message) {
        const decoded_string = utf8decoder.decode(message);
        const new_status = JSON.parse(decoded_string);

        connection_checker.beat();

        let status_list = $("<ul>");

        let run_time = 1;
        let run_time_minutes = 1;

        if (_.has(new_status, "runtime")) {
            run_time = dayjs.duration(new_status["runtime"], "seconds");
            run_time_minutes = run_time.asSeconds() / 60.0;

            $("<li>").text("Saving time: " + humanizeDuration(run_time.asMilliseconds()) + " (" + run_time.asSeconds() + " s)").appendTo(status_list);
        } else {
            $("<li>").text("Saving time: none").appendTo(status_list);
        }


        //let events_li = $("<li>").text("Run time: ");
        let events_li = $("<li>").text("Events:");
        let events_ul = $("<ul>");

        if (new_status.hasOwnProperty("events_file_opened")) {
            const file_opened = new_status["events_file_opened"];

            let li = $("<li>").text('File opened: ' + file_opened);

            if (file_opened) {
                const file_size = new_status["events_file_size"];
                const file_size_per_minute = Math.round(file_size / run_time_minutes);

                li.addClass("good_status").appendTo(events_ul);

                $('<li>').text("File name: " + new_status["events_file_name"]).appendTo(events_ul);
                $('<li>').text("File size: " + filesize(file_size).human()).appendTo(events_ul);
                $('<li>').text("File growth: " + filesize(file_size_per_minute).human() + "/min").appendTo(events_ul);
            } else {
                li.addClass("bad_status").appendTo(events_ul);
            }
        } else {
            $("<li>").text('File opened: Status Error').addClass("bad_status").appendTo(events_ul);
        }

        events_li.append(events_ul);
        status_list.append(events_li);

        let waveforms_li = $("<li>").text("Waveforms:");
        let waveforms_ul = $("<ul>");

        if (new_status.hasOwnProperty("waveforms_file_opened")) {
            const file_opened = new_status["waveforms_file_opened"];

            let li = $("<li>").text('File opened: ' + file_opened);

            if (file_opened) {
                const file_size = new_status["waveforms_file_size"];
                const file_size_per_minute = Math.round(file_size / run_time_minutes);

                li.addClass("good_status").appendTo(waveforms_ul);

                $('<li>').text("File name: " + new_status["waveforms_file_name"]).appendTo(waveforms_ul);
                $('<li>').text("File size: " + filesize(file_size).human()).appendTo(waveforms_ul);
                $('<li>').text("File growth: " + filesize(file_size_per_minute).human() + "/min").appendTo(waveforms_ul);
            } else {
                li.addClass("bad_status").appendTo(waveforms_ul);
            }
        } else {
            $("<li>").text('File opened: Status Error').addClass("bad_status").appendTo(waveforms_ul);
        }

        waveforms_li.append(waveforms_ul);
        status_list.append(waveforms_li);

        let raw_li = $("<li>").text("Raw:");
        let raw_ul = $("<ul>");

        if (new_status.hasOwnProperty("raw_file_opened")) {
            const file_opened = new_status["raw_file_opened"];

            let li = $("<li>").text('File opened: ' + file_opened);

            if (file_opened) {
                const file_size = new_status["raw_file_size"];
                const file_size_per_minute = Math.round(file_size / run_time_minutes);

                li.addClass("good_status").appendTo(raw_ul);

                $('<li>').text("File name: " + new_status["raw_file_name"]).appendTo(raw_ul);
                $('<li>').text("File size: " + filesize(file_size).human()).appendTo(raw_ul);
                $('<li>').text("File growth: " + filesize(file_size_per_minute).human() + "/min").appendTo(raw_ul);
            } else {
                li.addClass("bad_status").appendTo(raw_ul);
            }
        } else {
            $("<li>").text('File opened: Status Error').addClass("bad_status").appendTo(raw_ul);
        }

        raw_li.append(raw_ul);
        status_list.append(raw_li);

        $("#module_status").html(status_list);
    }

    function dasa_arguments() {
        var enable = {};
        enable["events"] = $("#file_enable_events").prop("checked");
        enable["waveforms"] = $("#file_enable_waveforms").prop("checked");
        enable["raw"] = $("#file_enable_raw").prop("checked");
    
        var kwargs = {"file_name": $("#file_name").val(), "enable": enable};
    
        return kwargs;
    }

    socket_io.on("connect", socket_io_connection(socket_io, module_name, on_status, update_events_log(), null));

    window.setInterval(function () {
        connection_checker.display();
    }, 1000);


    $("#button_start").on("click", send_command(socket_io, 'start', dasa_arguments));
    $("#button_stop").on("click", send_command(socket_io, 'stop'));
}

$(page_loaded);
