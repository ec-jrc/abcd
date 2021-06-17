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

function check_connection (timestamp) {
    // Expiration time in seconds
    const expiration = Math.abs(parseInt($("#expiration_time").val()));

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

var generate_update_tofcalc_status = function () {
    var old_status = {"timestamp": "###"};
    var old_channels_configs = {};

    return function (new_status) {
        if (new_status["timestamp"] !== old_status["timestamp"])
        {
            last_timestamp = new_status["timestamp"];

            console.log("Updating ToFcalc status");

            old_status = new_status;

            var selected_channel = parseInt($("#channel_select").val());
            //console.log("Selected channel: " + selected_channel);
            //console.log("New status: " + JSON.stringify(new_status));
            //console.log("Channels: " + new_status["active_channels"]);

            var channel_select = $("#channel_select").html('');

            try {
                const active_channels = new_status["active_channels"];

                active_channels.forEach(function (channel, index) {
                    if (channel === selected_channel) {
                        channel_select.append($('<option>', {value: channel, text: channel, selected: "selected"}));
                    } else {
                        channel_select.append($('<option>', {value: channel, text: channel}));
                    }
                });
            } catch (error) { }

            // Reread the selected channel after setting it, because on the
            // first update no channel will be selected by the for loop, but
            // the browser will do it automagically.
            selected_channel = parseInt($("#channel_select").val());

            //console.log("Selected channel: " + selected_channel);

            const new_channels_statuses = new_status["statuses"];

            var rates_list = $("#ToF_rates");
            rates_list.empty();

            new_channels_statuses.forEach(function (channel_status) {
                const channel = channel_status["id"];
                const rate = channel_status["rate"];

                rates_list.append($("<li>", {text: "Ch " + channel + ": " + rate.toFixed(2)}));
            });

            const new_channels_configs = new_status["configs"];

            if ((!_.isEqual(new_channels_configs, old_channels_configs))
                &&
                (!_.isNil(selected_channel))
                &&
                (_.isFinite(selected_channel))) {

                old_channels_configs = new_channels_configs;

                $("#tofcalc_general_status").empty();

                new_channels_configs.forEach(function (channel_config) {
                    const channel = channel_config.id || 0;
                    const histo_ToF_config = channel_config.ToF || {"min": 0, "max": 0, "bins": 0};
                    const histo_EToF_config = channel_config.EToF || {"min_x": 0, "max_x": 0, "bins_x": 0, "min_y": 0, "max_y": 0, "bins_y": 0};
                    console.log(histo_EToF_config);

                    var channel_display = $("<fieldset>");

                    channel_display.append($("<legend>", {text: "Channel: " + channel}));
                    channel_display.append($("<label>", {text: "ID: ", "for": "channel_id"}).addClass("hidden"));
                    channel_display.append($("<input>", {
                                                        type: "number",
                                                        value: channel,
                                                        name: "channel_id",
                                                        id: "channel_id_" + channel}).addClass("hidden").addClass("channel_ids"));
                    channel_display.append($("<br>"));
                    channel_display.append($("<label>", {text: "ToF min: ", "for": "ToF_min"}));
                    channel_display.append($("<input>", {
                                                        type: "number",
                                                        value: histo_ToF_config.min,
                                                        name: "ToF_min",
                                                        id: "ToF_min_" + channel}));
                    channel_display.append($("<br>"));
                    channel_display.append($("<label>", {text: "ToF max: ", "for": "ToF_max"}));
                    channel_display.append($("<input>", {
                                                        type: "number",
                                                        value: histo_ToF_config.max,
                                                        name: "ToF_max",
                                                        id: "ToF_max_" + channel}));
                    channel_display.append($("<br>"));
                    channel_display.append($("<label>", {text: "ToF bins: ", "for": "ToF_bins"}));
                    channel_display.append($("<input>", {
                                                        type: "number",
                                                        value: histo_ToF_config.bins,
                                                        name: "ToF_bins",
                                                        id: "ToF_bins_" + channel}));
                    channel_display.append($("<br>"));
                    channel_display.append($("<label>", {text: "Energy min: ", "for": "E_min"}));
                    channel_display.append($("<input>", {
                                                        type: "number",
                                                        value: histo_EToF_config.min_y,
                                                        name: "E_min",
                                                        id: "E_min_" + channel}));
                    channel_display.append($("<br>"));
                    channel_display.append($("<label>", {text: "Energy max: ", "for": "E_max"}));
                    channel_display.append($("<input>", {
                                                        type: "number",
                                                        value: histo_EToF_config.max_y,
                                                        name: "E_max",
                                                        id: "E_max_" + channel}));
                    channel_display.append($("<br>"));
                    channel_display.append($("<label>", {text: "Energy bins: ", "for": "E_bins"}));
                    channel_display.append($("<input>", {
                                                        type: "number",
                                                        value: histo_EToF_config.bins_y,
                                                        name: "E_bins",
                                                        id: "E_bins_" + channel}));
                    channel_display.append($("<br>"));

                    channel_display.append($("<span>").text("Reference:"));
                    channel_display.append(generate_slider("ToF_reference_" + channel, channel_config.reference));

                    $("#tofcalc_general_status").append(channel_display);
                });
            }
        }
    }
}

var update_gui_status = function (new_status) {
    var sockets_list = $("<ol>");
    const sockets = new_status["sockets"];

    var counter = 0;

    for (var id in sockets) {
        if (sockets.hasOwnProperty(id)) {
            const socket = sockets[id];

            var text = "ID: " + id + " connection time: " + socket["connection_time"];

            if (socket["connected"]) {
                counter += 1;
            } else {
                text += " disconnection time: " + socket["disconnection_time"];
            }

            var li = $("<li>").text(text);

            if (socket["connected"]) {
                li.addClass("good_status");
            } else {
                li.addClass("bad_status");
            }

            sockets_list.append(li);
        }
    }

    $("#gui_general_status").html($("<div>").text("Connected clients: " + counter));
    $("#gui_general_status").append(sockets_list);
};
