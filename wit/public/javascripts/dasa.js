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

        let overall_file_size = 0;

        let files_li = $("<li>").text("File types:");
        const files_ul = $("<ul>");

        for (const file_type of ["events", "waveforms", "raw"]) {
            let events_li = $("<li>").text(file_type.charAt(0).toUpperCase() + file_type.slice(1) + " file:");
            let events_ul = $("<ul>");

            if (new_status.hasOwnProperty(file_type + "_file_opened")) {
                const file_opened = new_status[file_type + "_file_opened"];

                let li = $("<li>").text('File opened: ' + file_opened);

                if (file_opened) {
                    const file_size = new_status[file_type + "_file_size"];
                    const file_size_per_minute = Math.round(file_size / run_time_minutes);

                    overall_file_size += file_size;

                    li.addClass("good_status").appendTo(events_ul);

                    $('<li>').text("File name: " + new_status[file_type + "_file_name"]).appendTo(events_ul);
                    $('<li>').text("File size: " + filesize(file_size).human()).appendTo(events_ul);
                    $('<li>').text("File growth: " + filesize(file_size_per_minute).human() + "/min").appendTo(events_ul);
                } else {
                    li.addClass("bad_status").appendTo(events_ul);
                }
            } else {
                $("<li>").text('File opened: Status Error').addClass("bad_status").appendTo(events_ul);
            }

            events_li.append(events_ul);
            files_ul.append(events_li);
        }

        files_li.append(files_ul);
        status_list.append(files_li);

        if (overall_file_size > 0) {
            let overall_li = $("<li>").text("Overall:");
            let overall_ul = $("<ul>");

            const overall_file_size_per_second = Math.round(overall_file_size / run_time.asSeconds());
            const overall_file_size_per_minute = Math.round(overall_file_size / run_time_minutes);

            $('<li>').text("Overall files size: " + filesize(overall_file_size).human()).appendTo(overall_ul);
            $('<li>').text("Overall files growth: " + filesize(overall_file_size_per_minute).human() + "/min").appendTo(overall_ul);

            if (new_status.hasOwnProperty("filesystem_capacity")
                && new_status.hasOwnProperty("filesystem_available")) {

                const fs_capacity = new_status["filesystem_capacity"];
                const fs_available = new_status["filesystem_available"];
                const fs_remaining_time = Math.round(fs_available / overall_file_size_per_second);

                $('<li>').text("Filesystem available space: " + filesize(fs_available).human() + " (total: " + filesize(fs_capacity).human() + ")").appendTo(overall_ul);
                $("<li>").text("Time to full: " + humanizeDuration(fs_remaining_time * 1000) + " (" + fs_remaining_time + " s)").appendTo(overall_ul);
            }

            overall_li.append(overall_ul);
            status_list.append(overall_li);
        }

        if (_.has(new_status, "work_directory")) {
            $("<li>").text(`Current work directory: ${new_status["work_directory"]}`).appendTo(status_list);
        }

        $("#module_status").html(status_list);
    }

    function dasa_arguments() {
        const enable = {
            "events": $("#file_enable_events").prop("checked"),
            "waveforms": $("#file_enable_waveforms").prop("checked"),
            "raw": $("#file_enable_raw").prop("checked")
        };

        let kwargs = { "enable": enable };

        let datetime = "";

        if ($("#file_automatic_name").prop("checked")) {
            datetime = "abcd_data_" + dayjs().format("YYYY-MM-DDTHH.mm.ssZZ") + "_";
        }

        const file_name = (String($("#file_name").val())).trim();

        if (file_name.length > 0) {
            kwargs["file_name"] = datetime + file_name;
        }

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
