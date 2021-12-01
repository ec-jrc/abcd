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

    const default_time_refresh = 3;
    const default_plot_height = 900;

    const layout_waveform = {
        xaxis: {
            title: 'Time [ch]',
            autorange: true,
            autotick: true,
            showspikes: true,
            spikemode: 'across',
            //linecolor: 'black',
            //linewidth: 1,
            //mirror: true,
            domain: [0.0, 1.0],
            anchor: 'y'
        },
        yaxis: {
            title: 'ADC value [ch]',
            autorange: true,
            autotick: true,
            showspikes: true,
            spikemode: 'across',
            domain: [0.5, 1.0]
        },
        yaxis2: {
            title: 'Arbitrary units',
            autorange: true,
            autotick: true,
            showspikes: true,
            spikemode: 'across',
            domain: [0.0, 0.4]
        },
        margin: {
           t: 10,
           l: 70,
           r: 10
        },
        showlegend: true,
        legend: {
            x: 1,
            xanchor: 'right',
            y: 0.75,
            yanchor: 'middle',
        },
        grid: {
            rows: 2,
            columns: 1,
            roworder:'top to bottom'
        },
        hovermode: 'closest',
        showlegend: true
    };
    const config_waveform = {
        responsive: true
    }

    var socket_io = io();

    var next_update_plot = dayjs().add(-10, "seconds");

    var active_channels = [];
    var waveforms = {};

    const module_name = String($('input#module_name').val());
    
    console.log("Module name: " + module_name);

    $("#time_refresh").val(default_time_refresh);

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

    function create_plot() {
        Plotly.newPlot('plot_waveform', [], layout_waveform, config_waveform);
    }

    function update_plot(force) {
        const force_update = !_.isNil(force);

        const now = dayjs();
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
                    name: 'Additional ' + gate_index,
                    type: 'scatter'
                });

                gate_index += 1;
            }

            Plotly.react('plot_waveform', data, layout_waveform);

            const refresh_time = Number($("#time_refresh").val());
            next_update_plot = dayjs().add(refresh_time, "seconds");
        }
    }

    function on_data(message) {
        const decoded_string = utf8decoder.decode(message);
        const new_waveforms = JSON.parse(decoded_string);
        add_to_waveforms(new_waveforms);
        update_selector(active_channels);
        update_plot();
    }

    function download_spectrum_data() {
        try {
            const this_selected_channel = selected_channel();
            const timestamp = waveforms[this_selected_channel].timestamp;
            const wave = waveforms[this_selected_channel];

            let csv_text = "#Waveform channel: " + this_selected_channel + " timestamp: " + timestamp + " created on: " + dayjs().format() + "\r\n";
            csv_text += "#index,sample"; 

            for (let inner_index = 0; inner_index < waveforms[this_selected_channel].gates.length; inner_index ++) {
                csv_text += ",additional" + inner_index;
            }

            csv_text += "\r\n";
        
            for (let index = 0; index < wave.samples.length; index ++) {
                const sample = wave.samples[index];

                csv_text += "" + index + "," + sample;

                for (let inner_index = 0; inner_index < wave.gates.length; inner_index ++) {
                    const gate = wave.gates[inner_index][index];
                    csv_text += "," + gate;
                }

                csv_text += "\r\n";
            }
        
            create_and_download_file(csv_text, "Waveform of channel " + this_selected_channel + " with timestamp " + timestamp + ".csv", "txt");
        } catch (error) {
            console.error(error);
        }
    }

    function reset_channels() {
        waveforms = {};
        active_channels = [];
        update_selector(active_channels);
        Plotly.react('plot_waveform', [], layout_waveform);
    }

    socket_io.on("connect", socket_io_connection(socket_io, module_name, null, null, on_data));

    $("#channel_select").on('change', function () {
        console.log("Changed channel");
        update_plot(true);
    });

    $("#time_refresh").on('change', function () {
        const refresh_time = Number($("#time_refresh").val());
        next_update_plot = dayjs().add(refresh_time, "seconds");
        update_plot();
    });

    $("#download_waveform").on("click", download_spectrum_data);
    $("#reset_all").on("click", reset_channels);

    // This part is to trigger a window resize to convince plotly to resize the plot
    var observer = new MutationObserver(function(mutations) {
        window.dispatchEvent(new Event('resize'));
    });
      
    observer.observe($("#plot_waveform")[0], {attributes: true})

    // Then force a resize...
    $("#plot_waveform").css("height", "" + default_plot_height + "px");

    create_plot();
    
}

$(page_loaded);
