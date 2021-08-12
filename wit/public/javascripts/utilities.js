// (C) Copyright 2016,2021 Cristiano Lino Fontana
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

function create_and_download_file (contents, file_name, file_type) {
    const data = new Blob([contents], {type: 'text/' + file_type});

    var click_event = new MouseEvent("click", {"view": window,
                                               "bubbles": true,
                                               "cancelable": false});

    var a = document.createElement('a');
    a.href = window.URL.createObjectURL(data);
    a.download = file_name;
    a.textContent = 'Download file!';
    a.dispatchEvent(click_event);
}

function check_connection (timestamp) {
    // Expiration time in seconds
    const expiration = Math.abs(Number($("#time_expiration").val()));

    const now = moment();
    const then = moment(timestamp);

    const difference = now.diff(then, 'seconds');

    //console.log("Expiration: " + expiration + "; difference: " + difference);

    if (difference < expiration) {
        return true;
    } else {
        return false;
    }
}

function display_connection (module) {
    const connected = check_connection(last_timestamp);
    const then = moment(last_timestamp);

    //console.log("Module: " + module + "; last timestamp: " + last_timestamp + "; connected: " + connected);

    $("#" + module + "_connection_status").removeClass();
    $("#" + module + "_general_status").removeClass();

    if (connected) {
        $("#" + module + "_connection_status").addClass("good_status").text('Connection OK');
    } else if (then.isValid()) {
        $("#" + module + "_general_status").addClass("unknown_status");
        $("#" + module + "_connection_status").addClass("bad_status").text('Connection lost, last update: ' + then.format('LLL'));
    } else {
        $("#" + module + "_general_status").addClass("unknown_status");
        $("#" + module + "_connection_status").addClass("unknown_status").text('No connection');
    }
}