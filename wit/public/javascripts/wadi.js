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

"use strict";

function page_loaded() {
    const default_time_refresh = 3;

    const layout_waveform = {
        yaxis: {
            autorange: true,
            showpikes: true,
            //tick0: 0
            autotick: true
        },
        margin: {
           t: 10,
           l: 50,
           r: 10
        },
        xaxis: {
            autorange: true,
            showpikes: true,
            autotick: true
        },
        grid: {
            rows: 2,
            columns: 1,
            subplots:[['xy'], ['xy2']],
            roworder:'top to bottom'
        },
        hovermode: 'closest',
        showlegend: true
    };
    const config_waveform = {
        responsive: true
    }

    var socket_io = io();

    const update_delay = 2;
    var next_update_plot = moment().add(-10, "seconds");

    var active_channels = [];
    var waveforms = {};

    const module_name = String($('input#module_name').val());
    
    console.log("Module name: " + module_name);

    function add_to_waveforms(message) {
        //console.log("timestamp: " + message.timestamp);

        if (_.has(message, "active_channels") && _.has(message, "channels"))
        {
            for (let active_channel of message.active_channels) {
                //console.log("active_channel: " + active_channel);

                active_channels.push(active_channel);

                waveforms[active_channel] = _.find(message.channels, v => (v.id === active_channel));
            }

            active_channels = _.sortBy(_.uniq(active_channels));
        }
    }

    function selected_channel() {
        return parseInt($("#channel_select").val());
    }

    function update_selector() {
        let do_update = false;

        if ($('#channel_select option').length !== active_channels.length) {
            do_update = true;
        } else {
            const old_active = $("#channel_select option").map((index, element) => Number($(element).val())).toArray();

            if (!_.isEqual(active_channels, old_active)) {
                do_update = true;
            }
        }

        if (do_update) {
            //console.log("Updating selector");

            const this_selected_channel = selected_channel();

            let channel_select = $("#channel_select").html('');

            for (let active_channel of active_channels) {
               if (active_channel === this_selected_channel) {                                      
                   channel_select.append($('<option>', {value: active_channel, text: String(active_channel), selected: "selected"}));
               } else {                                                         
                   channel_select.append($('<option>', {value: active_channel, text: String(active_channel)}));                                    
               }                      
            }
        }
    }

    function create_plot() {
        Plotly.newPlot('plot_waveform', [], layout_waveform, config_waveform);
    }

    function update_plot(force) {
        const force_update = !_.isNil(force);

        const now = moment();
        const difference = next_update_plot.diff(now, "seconds");

        const can_update = $('#update_plot').is(":checked");

        if ((difference <= 0 && can_update) || force_update) {
            //console.log("Updating plot");

            const timestamp = waveforms[selected_channel()].timestamp;

            $("#timestamp_display").text(timestamp)

            const samples = waveforms[selected_channel()].samples;

            const waveform = {
                y: samples,
                xaxis: 'x',
                yaxis: 'y',
                mode: 'lines',
                line: {shape: 'hv'},
                name: 'Samples',
                type: 'scatter'
            };

            const data = [waveform];

            let gate_index = 0;

            for (let gate of waveforms[selected_channel()].gates) {
                data.push({
                    y: gate,
                    xaxis: 'x',
                    yaxis: 'y2',
                    mode: 'lines',
                    line: {shape: 'hv'},
                    name: 'Gate ' + gate_index,
                    type: 'scatter'
                });

                gate_index += 1;
            }

            Plotly.react('plot_waveform', data, layout_waveform);

            const refresh_time = Number($("#time_refresh").val());
            next_update_plot = moment().add(refresh_time, "seconds");
        }
    }

    function socket_io_connection() {
        console.log("Connected to wit server");

        $('#wit_connection_status').removeClass();
        $('#wit_connection_status').addClass("good_status").text("Connection OK");
    
        //socket_io.on('event_gui', generate_update_events("gui"));
    
        socket_io.emit('join_module', (function () {
            console.log("Requesting to join a room with name: " + module_name);
            return module_name;
        })());

        socket_io.on('acknowledge', function (message) {
            console.log("Acknowledge message: " + message);
        });

        socket_io.on('ping', function (message) {
            socket_io.emit('pong', { "timestamp" : moment() });
        });

        socket_io.on('data', function (message) {
            let utf8decoder = new TextDecoder("utf8");
            const decoded_string = utf8decoder.decode(message);
            const new_waveforms = JSON.parse(decoded_string);
            add_to_waveforms(new_waveforms);
            update_selector();
            update_plot();
        });
    
        socket_io.on('disconnect', socket_io_disconnection);
    }

    
    function socket_io_disconnection(error) {
        console.log("Server disconnected, error: " + error);
    
        $('#wit_connection_status').removeClass().addClass("unknown_status");
        $('#wit_connection_status').text("No connection");
    }
    
    socket_io.on("connect", socket_io_connection);

    $("#channel_select").on('change', function () {
        console.log("Changed channel");
        update_plot(true);
    });

    $("#time_refresh").on('change', function () {
        const refresh_time = Number($("#time_refresh").val());
        next_update_plot = moment().add(refresh_time, "seconds");
        update_plot();
    });

    $("#download_waveform").on("click", function () {
        try {
            const this_selected_channel = selected_channel();
            const timestamp = waveforms[this_selected_channel].timestamp;

            let csv_text = "#Waveform channel: " + this_selected_channel + " timestamp: " + timestamp + "\r\n";
            csv_text += "#index,sample"; 

            for (let inner_index = 0; inner_index < waveforms[this_selected_channel].gates.length; inner_index ++) {
                csv_text += ",gate" + inner_index;
            }

            csv_text += "\r\n";
        
            for (let index = 0; index < waveforms[this_selected_channel].samples.length; index ++) {
                const sample = waveforms[this_selected_channel].samples[index];

                csv_text += "" + index + "," + sample;

                for (let inner_index = 0; inner_index < waveforms[this_selected_channel].gates.length; inner_index ++) {
                    const gate = waveforms[this_selected_channel].gates[inner_index][index];
                    csv_text += "," + gate;
                }

                csv_text += "\r\n";
            }
        
            create_and_download_file(csv_text, "waveform_channel" + this_selected_channel + "_timestamp" + timestamp + ".csv", "txt");
        } catch (error) {
            console.error(error);
        }
    });

    $("#time_refresh").val(default_time_refresh);

    create_plot();
    
}

$(page_loaded);
