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
    const expiration = parseInt($("#expiration_time").val());

    const now = moment();
    const then = moment(timestamp);

    if (now.diff(then, 'seconds') < expiration) {
        return true;
    } else {
        return false;
    }
}

function display_connection (module) {
    const connected = check_connection(last_timestamps[module], module);
    const then = moment(last_timestamps[module]);

    $("#" + module + "_connection_status").removeClass();

    if (connected) {
        $("#" + module + "_general_status").removeClass();
        $("#" + module + "_connection_status").addClass("good_status").text('Connection OK');
    } else if (then.isValid()) {
        $("#" + module + "_general_status").removeClass().addClass("unknown_status");
        $("#" + module + "_connection_status").addClass("bad_status").text('Connection lost, last update: ' + then.format('LLL'));
    } else {
        $("#" + module + "_general_status").removeClass().addClass("unknown_status");
        $("#" + module + "_connection_status").removeClass().addClass("unknown_status");
        $("#" + module + "_connection_status").text('No connection');
    }
}

var update_abcd_status = (function () {
    var old_status = {"timestamp": "###"};

    return function (new_status) {
        if (new_status["timestamp"] !== old_status["timestamp"])
        {
            last_timestamps["abcd"] = new_status["timestamp"];

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
                $("<li>").text('Valid pointer: ' + new_status["digitizer"]["valid_pointer"]).addClass("good_status").appendTo(status_list);
            } else {
                $("<li>").text('Digitizer active: Status Error').addClass("bad_status").appendTo(status_list);
                $("<li>").text('Valid pointer: Status Error').addClass("bad_status").appendTo(status_list);
            }

            if (new_status.hasOwnProperty("acquisition")) {
                var running = new_status["acquisition"]["running"];

                if (running) {
                    $("<li>").text('Acquisition running: ' + running).addClass("good_status").appendTo(status_list);

                    var run_time = moment.duration(new_status["acquisition"]["runtime"], "seconds");
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

            $("#abcd_general_status").html(status_list);
        }
    }
})();

var update_hijk_status = (function () {
    var old_status = {"timestamp": "###"};
    var old_configuration = {};

    return function (new_status) {
        if (new_status["timestamp"] !== old_status["timestamp"])
        {
            last_timestamps["hijk"] = new_status["timestamp"];

            old_status = new_status;

            var status_list = $("<ul>");

            if (new_status.hasOwnProperty("config")) {
                if (new_status["config"].hasOwnProperty("id")) {
                    $("<li>").text('Module ID: ' + new_status["config"]["id"]).appendTo(status_list);
                }
            }

            if (new_status.hasOwnProperty("hv_card")) {
                $("<li>").text('HV active: ' + new_status["hv_card"]["active"]).addClass("good_status").appendTo(status_list);
                $("<li>").text('Valid pointer: ' + new_status["hv_card"]["valid_pointer"]).addClass("good_status").appendTo(status_list);

                if (new_status["hv_card"].hasOwnProperty("channels")) {
                    var channels_number = new_status["hv_card"]["channels"].length;

                    var channels_list = $("<ol>");
                    channels_list.attr("start", 0);

                    for (var channel = 0; channel < channels_number; channel++) {
                        var channel_status = new_status["hv_card"]["channels"][channel];

                        var channel_display = $("<ul>");
                        $("<li>").text("ID: " + channel_status["id"]).appendTo(channel_display);
                        $("<li>").text("Voltage: " + channel_status["voltage"].toFixed(0)).appendTo(channel_display);
                        $("<li>").text("Current: " + channel_status["current"].toFixed(1)).appendTo(channel_display);
                        $("<li>").text("On: " + channel_status["on"]).appendTo(channel_display);
                        $("<li>").text("Status: " + channel_status["status"]).appendTo(channel_display);
                        $("<li>").append(channel_display).appendTo(channels_list);

                    }

                    var li = $("<li>").text("Channels:");

                    li.append(channels_list);
                    status_list.append(li);
                }

            } else {
                $("<li>").text('HV active: Status Error').addClass("bad_status").appendTo(status_list);
                $("<li>").text('Valid pointer: Status Error').addClass("bad_status").appendTo(status_list);
            }

            if (new_status.hasOwnProperty("config")) {
                const new_configuration = new_status["config"];

                if (!_.isEqual(old_configuration, new_configuration)) {
                    old_configuration = new_configuration;

                    if (new_configuration.hasOwnProperty("channels")) {

                        var card_display = $("<fieldset>");

                        card_display.append($("<legend>", {text: "HV Card"}));
                        card_display.append($("<label>", {text: "Model number: ", "for": "model_number"}));
                        card_display.append($("<input>", {
                                                            type: "number",
                                                            value: new_configuration["model_number"],
                                                            name: "model_number", 
                                                            id: "model_number"}));
                        card_display.append($("<br>"));
                        card_display.append($("<label>", {text: "Connection type: ", "for": "connection_type"}));
                        var connection_type_select = $("<select>", {
                                                            name: "connection_type", 
                                                            id: "connection_type"});
                        connection_type_select.append($('<option>', {value: "USB", text: "USB", selected: "selected"}));
                        connection_type_select.append($('<option>', {value: "OpticalLink", text: "Optical Link"}));
                        card_display.append(connection_type_select);
                        card_display.append($("<br>"));
                        card_display.append($("<label>", {text: "Link number: ", "for": "link_number"}));
                        card_display.append($("<input>", {
                                                            type: "number",
                                                            value: new_configuration["link_number"],
                                                            name: "link_number", 
                                                            id: "link_number"}));
                        card_display.append($("<br>"));
                        card_display.append($("<label>", {text: "CONET node: ", "for": "CONET_node"}));
                        card_display.append($("<input>", {
                                                            type: "number",
                                                            value: new_configuration["CONET_node"],
                                                            name: "CONET_node", 
                                                            id: "CONET_node"}));
                        card_display.append($("<br>"));
                        card_display.append($("<label>", {text: "VME address: ", "for": "VME_address"}));
                        card_display.append($("<input>", {
                                                            type: "text",
                                                            value: new_configuration["VME_address"],
                                                            name: "VME_address", 
                                                            id: "VME_address"}));
                        card_display.append($("<br>"));
                        card_display.append($("<label>", {text: "Channels: ", "for": "hv_channels"}).addClass("hidden"));
                        card_display.append($("<input>", {
                                                            type: "number",
                                                            value: new_configuration["channels"].length,
                                                            name: "hv_channels", 
                                                            id: "hv_channels"}).addClass("hidden"));

                        $("#commands_hijk").html(card_display);

                        const channels_number = new_configuration["channels"].length;

                        for (var channel = 0; channel < channels_number; channel++) {
                            const channel_config = new_configuration["channels"][channel];

                            var channel_display = $("<fieldset>");

                            channel_display.append($("<legend>", {text: "Channel: " + channel_config["id"]}));
                            channel_display.append($("<label>", {text: "ID: ", "for": "channel_id"}).addClass("hidden"));
                            channel_display.append($("<input>", {
                                                                type: "number",
                                                                value: channel_config["id"],
                                                                name: "channel_id", 
                                                                id: "channel_id_" + channel}).addClass("hidden"));
                            channel_display.append($("<br>"));
                            channel_display.append($("<label>", {text: "Max Voltage: ", "for": "max_voltage"}));
                            channel_display.append($("<input>", {
                                                                type: "number",
                                                                value: channel_config["max_voltage"],
                                                                name: "max_voltage", 
                                                                id: "max_voltage_" + channel}));
                            channel_display.append($("<br>"));
                            channel_display.append($("<label>", {text: "Voltage: ", "for": "voltage"}));
                            channel_display.append($("<input>", {
                                                                type: "number",
                                                                value: channel_config["voltage"],
                                                                name: "voltage", 
                                                                id: "voltage_" + channel}));
                            channel_display.append($("<br>"));
                            channel_display.append($("<label>", {text: "Max current: ", "for": "max_current"}));
                            channel_display.append($("<input>", {
                                                                type: "number",
                                                                value: channel_config["max_current"],
                                                                name: "max_current", 
                                                                id: "max_current_" + channel}));
                            channel_display.append($("<br>"));
                            channel_display.append($("<label>", {text: "Ramp up: ", "for": "ramp_up"}));
                            channel_display.append($("<input>", {
                                                                type: "number",
                                                                value: channel_config["ramp_up"],
                                                                name: "ramp_up", 
                                                                id: "ramp_up_" + channel}));
                            channel_display.append($("<br>"));
                            channel_display.append($("<label>", {text: "Ramp down: ", "for": "ramp_down"}));
                            channel_display.append($("<input>", {
                                                                type: "number",
                                                                value: channel_config["ramp_down"],
                                                                name: "ramp_down", 
                                                                id: "ramp_down_" + channel}));
                            channel_display.append($("<br>"));
                            channel_display.append($("<span>").text("On/Off state:"));
                            channel_display.append(generate_slider("channel_on_" + channel, channel_config["on"]));
    
                            $("#commands_hijk").append(channel_display);
                        }
                    }
                }
            }

            $("#hijk_general_status").html(status_list);
        }
    }
})();

function file_size_to_human(size) {
    const num_size = Math.abs(Number(size));

    if (size == 0) {
        return "0 B";
    } else {
        const exponent = Math.floor(Math.log(size) / Math.log(1024));
        const mantissa = size / Math.pow(1024, exponent);
        
        return mantissa.toFixed(2) + ' ' + ['B', 'kiB', 'MiB', 'GiB', 'TiB', 'PiB'][exponent];
    }
};

var update_lmno_status = (function () {
    var old_status = {"timestamp": "###"};

    return function (new_status) {
        if (new_status["timestamp"] !== old_status["timestamp"])
        {
            last_timestamps["lmno"] = new_status["timestamp"];

            old_status = new_status;

            var status_list = $("<ul>");

            //if (new_status.hasOwnProperty("config")) {
            //    if (new_status["config"].hasOwnProperty("id")) {
            //        $("<li>").text('Module ID: ' + new_status["config"]["id"]).appendTo(status_list);
            //    }
            //}

            try {
                var run_time = moment.duration(new_status["runtime"], "seconds");
                $("<li>").text("Saving time: " + humanizeDuration(run_time.asMilliseconds()) + " (" + run_time.asSeconds() + " s)").appendTo(status_list);
            } catch (error) { }

            var events_li = $("<li>").text("Run time: ");
            var events_li = $("<li>").text("Events:");
            var events_ul = $("<ul>");

            if (new_status.hasOwnProperty("events_file_opened")) {
                var file_opened = new_status["events_file_opened"];

                var li = $("<li>").text('File opened: ' + file_opened);

                if (file_opened) {
                    li.addClass("good_status").appendTo(events_ul);

                    $('<li>').text("File name: " + new_status["events_file_name"]).appendTo(events_ul);
                    $('<li>').text("File size: " + file_size_to_human(new_status["events_file_size"])).appendTo(events_ul);
                } else {
                    li.addClass("bad_status").appendTo(events_ul);
                }
            } else {
                $("<li>").text('File opened: Status Error').addClass("bad_status").appendTo(events_ul);
            }

            events_li.append(events_ul);
            status_list.append(events_li);

            var waveforms_li = $("<li>").text("Waveforms:");
            var waveforms_ul = $("<ul>");

            if (new_status.hasOwnProperty("waveforms_file_opened")) {
                var file_opened = new_status["waveforms_file_opened"];

                var li = $("<li>").text('File opened: ' + file_opened);

                if (file_opened) {
                    li.addClass("good_status").appendTo(waveforms_ul);

                    $('<li>').text("File name: " + new_status["waveforms_file_name"]).appendTo(waveforms_ul);
                    $('<li>').text("File size: " + file_size_to_human(new_status["waveforms_file_size"])).appendTo(waveforms_ul);
                } else {
                    li.addClass("bad_status").appendTo(waveforms_ul);
                }
            } else {
                $("<li>").text('File opened: Status Error').addClass("bad_status").appendTo(waveforms_ul);
            }

            waveforms_li.append(waveforms_ul);
            status_list.append(waveforms_li);

            var raw_li = $("<li>").text("Raw:");
            var raw_ul = $("<ul>");

            if (new_status.hasOwnProperty("raw_file_opened")) {
                var file_opened = new_status["raw_file_opened"];

                var li = $("<li>").text('File opened: ' + file_opened);

                if (file_opened) {
                    li.addClass("good_status").appendTo(raw_ul);

                    $('<li>').text("File name: " + new_status["raw_file_name"]).appendTo(raw_ul);
                    $('<li>').text("File size: " + file_size_to_human(new_status["raw_file_size"])).appendTo(raw_ul);
                } else {
                    li.addClass("bad_status").appendTo(raw_ul);
                }
            } else {
                $("<li>").text('File opened: Status Error').addClass("bad_status").appendTo(raw_ul);
            }

            raw_li.append(raw_ul);
            status_list.append(raw_li);

            $("#lmno_general_status").html(status_list);
        }
    }
})();

var update_pqrs_status = (function () {
    var old_status = {"timestamp": "###"};
    var old_config = {};

    return function (new_status) {
        if (new_status["timestamp"] !== old_status["timestamp"])
        {
            last_timestamps["pqrs"] = new_status["timestamp"];

            //console.log("Updating PQRS status");

            old_status = new_status;

            const new_config = new_status["configs"];

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

            if ((!_.isEqual(new_config, old_config))
                &&
                (!_.isNil(selected_channel))
                &&
                (_.isFinite(selected_channel))) {

                old_config = new_config;

                $("#pqrs_general_status").empty();

                //if (new_status.hasOwnProperty("config")) {
                //    if (new_status["config"].hasOwnProperty("id")) {
                //        $("<div>").text('Module ID: ' + new_status["config"]["id"]).appendTo($("#pqrs_general_status"));
                //    }
                //}

                const qlong_config = new_config["qlong"][selected_channel];
                const PSD_config = new_config["PSD"][selected_channel];

                var qlong_config_display = $("<fieldset>");

                qlong_config_display.append($("<legend>", {text: "q long settings"}));
                qlong_config_display.append($("<label>", {text: "Bins", "for": "qlong_bins"}));
                qlong_config_display.append($("<input>", {
                                                    type: "number",
                                                    value: qlong_config["bins"],
                                                    name: "qlong_bins", 
                                                    id: "qlong_bins"}));
                qlong_config_display.append($("<br>"));
                qlong_config_display.append($("<label>", {text: "Min", "for": "qlong_min"}));
                qlong_config_display.append($("<input>", {
                                                    type: "number",
                                                    value: qlong_config["min"],
                                                    name: "qlong_min", 
                                                    id: "qlong_min"}));
                
                qlong_config_display.append($("<br>"));
                qlong_config_display.append($("<label>", {text: "Max", "for": "qlong_max"}));
                qlong_config_display.append($("<input>", {
                                                    type: "number",
                                                    value: qlong_config["max"],
                                                    name: "qlong_max", 
                                                    id: "qlong_max"}));
                
                var PSD_config_display = $("<fieldset>");

                PSD_config_display.append($("<legend>", {text: "PSD settings"}));
                PSD_config_display.append($("<label>", {text: "qlong Bins", "for": "psd_qlong_bins"}));
                PSD_config_display.append($("<input>", {
                                                        type: "number",
                                                        value: PSD_config["bins_x"],
                                                        name: "psd_qlong_bins", 
                                                        id: "psd_qlong_bins"}));
                PSD_config_display.append($("<br>"));
                PSD_config_display.append($("<label>", {text: "qlong Max", "for": "psd_qlong_max"}));
                PSD_config_display.append($("<input>", {
                                                        type: "number",
                                                        value: PSD_config["max_x"],
                                                        step: 0.1,
                                                        name: "psd_qlong_max", 
                                                        id: "psd_qlong_max"}));
                
                PSD_config_display.append($("<br>"));
                PSD_config_display.append($("<label>", {text: "PSD Bins", "for": "psd_psd_bins"}));
                PSD_config_display.append($("<input>", {
                                                        type: "number",
                                                        value: PSD_config["bins_y"],
                                                        name: "psd_psd_bins", 
                                                        id: "psd_psd_bins"}));
                PSD_config_display.append($("<br>"));
                PSD_config_display.append($("<label>", {text: "PSD Min", "for": "psd_psd_min"}));
                PSD_config_display.append($("<input>", {
                                                        type: "number",
                                                        value: PSD_config["min_y"],
                                                        step: 0.1,
                                                        name: "psd_psd_min", 
                                                        id: "psd_psd_min"}));
                PSD_config_display.append($("<br>"));
                PSD_config_display.append($("<label>", {text: "PSD Max", "for": "psd_psd_max"}));
                PSD_config_display.append($("<input>", {
                                                        type: "number",
                                                        value: PSD_config["max_y"],
                                                        step: 0.1,
                                                        name: "psd_psd_max", 
                                                        id: "psd_psd_max"}));
                
                $("#pqrs_general_status").append(qlong_config_display);
                $("#pqrs_general_status").append(PSD_config_display);
            }
        }
    }
})();

var update_efg_status = function (new_status) {
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

    $("#efg_general_status").html($("<div>").text("Connected clients: " + counter));
    $("#efg_general_status").append(sockets_list);
};
