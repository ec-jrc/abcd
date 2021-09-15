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

"use strict";

dayjs.extend(window.dayjs_plugin_duration);
dayjs.extend(window.dayjs_plugin_localizedFormat);

function socket_io_connection(socket_io, module_name, on_status, on_events, on_data) {
    return function () {
        console.log("Connected to wit server");

        $('#wit_connection_status').removeClass().addClass("good_status").text("Connection OK");

        socket_io.emit('join_module', (function () {
            console.log("Requesting to join a room with name: " + module_name);
            return module_name;
        })());

        socket_io.on('acknowledge', function (message) {
            console.log("Acknowledge message: " + message);
        });

        socket_io.on('ping', function (message) {
            socket_io.emit('pong', { "timestamp" : dayjs() });
        });

        if (!_.isNil(on_status)) {
            socket_io.on('status', on_status);
        }
        if (!_.isNil(on_events)) {
            socket_io.on('events', on_events);
        }
        if (!_.isNil(on_data)) {
            socket_io.on('data', on_data);
        }

        socket_io.on('disconnect', socket_io_disconnection);
    }
}

function socket_io_disconnection(error) {
    console.log("Server disconnected, error: " + error);

    $('#wit_connection_status').removeClass().addClass("unknown_status").text("No connection");
}
    
function update_selector(active_channels) {
    const sorted_actives = _.sortBy(active_channels);

    let do_update = false;

    if ($('#channel_select option').length !== sorted_actives.length) {
        do_update = true;
    } else {
        const old_actives = _.sortBy($("#channel_select option").map((index, element) => Number($(element).val())).toArray());

        if (!_.isEqual(sorted_actives, old_actives)) {
            do_update = true;
        }
    }

    if (do_update) {
        //console.log("Updating selector");

        const this_selected_channel = selected_channel();

        let channel_select = $("#channel_select").html('');

        for (let active_channel of sorted_actives) {
           if (active_channel === this_selected_channel) {                                      
               channel_select.append($('<option>', {value: active_channel, text: String(active_channel), selected: "selected"}));
           } else {                                                         
               channel_select.append($('<option>', {value: active_channel, text: String(active_channel)}));                                    
           }                      
        }
    }
}

function selected_channel() {
    return parseInt($("#channel_select").val());
}

function update_events_log() {
    let previous_events = [];

    return function (events) {
        if (! _.isEqual(previous_events, events)) {
            previous_events = events;

            if (events.length > 0) {
                $("#events_log").text(" (" + events.length + ")");
            } else {
                $("#events_log").text('');
            }

            if (events.length > 0) {
                const message = events[0];

                const type = message["type"];
                const timestamp = dayjs(message["timestamp"]);
                const description = message[type];

                var counts = []
                counts[0] = {T: type, d: description, n: 1|0, t: timestamp};

                for (let i = 1; i < events.length; i++) {
                    const message = events[i];

                    const type = message["type"];
                    const timestamp = dayjs(message["timestamp"]);
                    const description = message[type];

                    const last_index = counts.length - 1;

                    if (counts[last_index].d === description)
                    {
                        counts[last_index].n += 1|0;
                        counts[last_index].t = timestamp;
                    }
                    else
                    {
                        counts.push({T: type, d: description, n: 1|0, t: timestamp});
                    }
                }

                let ol = $("<ol>");

                for (let i = 0; i < counts.length; i++) {
                    let li = $("<li>");

                    if (counts[i].n > 1) {
                        li.text("[" + counts[i].t.format('LLL') + "] " + counts[i].d + " (x " + counts[i].n + ")");
                    } else {
                        li.text("[" + counts[i].t.format('LLL') + "] " + counts[i].d);
                    }

                    if (counts[i].T === 'error') {
                        li.addClass("bad_status");
                    }

                    ol.append(li);
                }

                $("#events_log").html(ol);
            }
        }
    }
}

function send_command(socket_io, command, kwargs_generator) {
    let msg_counter = 0;

    return function () {
        let kwargs = null;

        if (!_.isNil(kwargs_generator)) {
            if (_.isFunction(kwargs_generator)) {
                kwargs = kwargs_generator();
            } else {
                kwargs = kwargs_generator;
            }
        }

        const now = dayjs().format();

        if (_.isNil(kwargs)) {
            console.log("Sending command: " + String(command));
        } else {
            console.log("Sending command: " + String(command) + " with arguments: " + JSON.stringify(kwargs))
        }

        const data = {"command": command,
                      "arguments": kwargs,
                      "msg_ID": msg_counter,
                      "timestamp": now
                     };

        socket_io.emit("command", JSON.stringify(data));

        msg_counter += 1;
    }
}

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

function generate_slider (id, checked) {
    var slider = $("<label>");
    slider.addClass("custom-slider");
    if (checked) {
        slider.append($("<input>", {type: "checkbox", id: id, checked: "checked"}));
    } else {
        slider.append($("<input>", {type: "checkbox", id: id}));
    }
    slider.append($("<span>").addClass("cursor"));

    return slider;
}

class ConnectionChecker {
    constructor() {
        this.last_timestamp = dayjs();
    }
    beat () {
        this.last_timestamp = dayjs();
    }
    check(timestamp) {
        const then = _.isNil(timestamp) ? this.last_timestamp : timestamp;

        // Expiration time in seconds
        const expiration = Math.abs(parseFloat($("#time_expiration").val()));

        const now = dayjs();

        const difference = now.diff(then, 'seconds');

        //console.log("Expiration: " + expiration + "; difference: " + difference);

        if (difference < expiration) {
            return true;
        } else {
            return false;
        }
    }
    display () {
        const connected = this.check();

        //console.log("Last timestamp: " + this.last_timestamp.format() + "; connected: " + connected);

        $("#module_connection_status").removeClass();
        $("#module_status").removeClass();

        if (connected) {
            $("#module_connection_status").addClass("good_status").text('Connection OK');
        } else {
            $("#module_status").addClass("unknown_status");
            $("#module_connection_status").addClass("unknown_status").text('No connection');
        }
    }
}