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

function page_loaded() {
    const utf8decoder = new TextDecoder("utf8");

    const default_time_refresh = 4;
    const default_plot_height = 900;

    var connection_checker = new ConnectionChecker();

    var updatemenus_spectrum = [
        {
            buttons: [
                {
                    args: ['yaxis.type', 'lin'],
                    label:'Linear Y',
                    method:'relayout'
                },
                {
                    args: ['yaxis.type', 'log'],
                    label: 'Logarithmic Y',
                    method: 'relayout'
                }
            ],
            direction: 'down',
            pad: {'r': 10, 't': 10},
            showactive: true,
            type: 'dropdown',
            x: 0.0,
            xanchor: 'left',
            y: 1.1,
            yanchor: 'bottom'
        }
    ]

    var layout_spectrum = {
        yaxis: {
            autorange: true,
            //tick0: 0
            autotick: true,
            showspikes: true,
            spikemode: 'across'
        },
        xaxis: {
            autorange: true,
            autotick: true,
            showspikes: true,
            spikemode: 'across'
        },
        updatemenus: updatemenus_spectrum,
        margin: {
           t: 10,
           l: 50,
           r: 10
        },
        grid: {
            rows: 2,
            columns: 1,
            subplots:[['xy'], ['xy2']],
            roworder:'top to bottom'
        },
        hovermode: 'closest'
    };

    const config_spectrum = {
        responsive: true
    }

    var socket_io = io();

    var next_update_plot = dayjs().add(-10, "seconds");

    var active_channels = [];
    var spectra = {};

    const module_name = String($('input#module_name').val());
    
    console.log("Module name: " + module_name);

    $("#time_refresh").val(default_time_refresh);

    var old_status = {"timestamp": "###"};
    var old_channels_configs = {};
    var last_timestamp = null;
    var last_abcd_config = null;

    function on_status(message) {
        const decoded_string = utf8decoder.decode(message);
        const new_status = JSON.parse(decoded_string);

        connection_checker.beat();

        if (new_status["timestamp"] !== old_status["timestamp"])
        {
            last_timestamp = new_status["timestamp"];

            old_status = new_status;

            //console.log("Updating Speccalc status");

            old_status = new_status;

            const this_selected_channel = selected_channel();

            //console.log("Selected channel: " + this_selected_channel);
            //console.log("New status: " + JSON.stringify(new_status));
            //console.log("Channels: " + new_status["active_channels"]);
            try {
                const active_channels = new_status["active_channels"];
                update_selector(active_channels);
            } catch (error) { }

            const new_channels_statuses = new_status["statuses"];

            let rates_list = $("#channels_rates");
            rates_list.empty();

            new_channels_statuses.forEach(function (channel_status) {
                const channel = channel_status["id"];
                const rate = channel_status["rate"];

                rates_list.append($("<li>", {text: "Ch " + channel + ": " + rate.toFixed(2)}));
            });

            const new_channels_configs = new_status["configs"];

            if ((!_.isEqual(new_channels_configs, old_channels_configs))
                &&
                (!_.isNil(this_selected_channel))
                &&
                (_.isFinite(this_selected_channel))) {

                old_channels_configs = new_channels_configs;

                $("#module_status").empty();

                new_channels_configs.forEach(function (channel_config) {
                    const channel = channel_config["id"];

                    let channel_display = $("<fieldset>");

                    channel_display.append($("<legend>", {text: "Channel: " + channel_config.id}));
                    channel_display.append($("<label>", {text: "ID: ", "for": "channel_id"}).addClass("hidden"));
                    channel_display.append($("<input>", {
                                                        type: "number",
                                                        value: channel_config.id,
                                                        name: "channel_id",
                                                        id: "channel_id_" + channel}).addClass("hidden").addClass("channel_ids"));
                    channel_display.append($("<br>"));
                    channel_display.append($("<label>", {text: "Energy min: ", "for": "energy_min"}));
                    channel_display.append($("<input>", {
                                                        type: "number",
                                                        value: channel_config.energy.min,
                                                        name: "energy_min",
                                                        id: "energy_min_" + channel}));
                    channel_display.append($("<br>"));
                    channel_display.append($("<label>", {text: "Energy max: ", "for": "energy_max"}));
                    channel_display.append($("<input>", {
                                                        type: "number",
                                                        value: channel_config.energy.max,
                                                        name: "energy_max",
                                                        id: "energy_max_" + channel}));
                    channel_display.append($("<br>"));
                    channel_display.append($("<label>", {text: "Energy bins: ", "for": "energy_bins"}));
                    channel_display.append($("<input>", {
                                                        type: "number",
                                                        value: channel_config.energy.bins,
                                                        name: "energy_bins",
                                                        id: "energy_bins_" + channel}));
                    channel_display.append($("<br>"));
                    channel_display.append($("<label>", {text: "PSD min: ", "for": "PSD_min"}));
                    channel_display.append($("<input>", {
                                                        type: "number",
                                                        value: channel_config.PSD.min_y,
                                                        name: "PSD_min",
                                                        id: "PSD_min_" + channel}));
                    channel_display.append($("<br>"));
                    channel_display.append($("<label>", {text: "PSD max: ", "for": "PSD_max"}));
                    channel_display.append($("<input>", {
                                                        type: "number",
                                                        value: channel_config.PSD.max_y,
                                                        name: "PSD_max",
                                                        id: "PSD_max_" + channel}));
                    channel_display.append($("<br>"));
                    channel_display.append($("<label>", {text: "PSD bins: ", "for": "PSD_bins"}));
                    channel_display.append($("<input>", {
                                                        type: "number",
                                                        value: channel_config.PSD.bins_y,
                                                        name: "PSD_bins",
                                                        id: "PSD_bins_" + channel}));
                    channel_display.append($("<br>"));

                    $("#module_status").append(channel_display);
                });
            }
        }
    }

    function spec_arguments_config() {
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

    function add_to_spectra(message) {
        //console.log("timestamp: " + message.timestamp);

        if (_.has(message, "active_channels") && _.has(message, "data"))
        {
            for (let active_channel of message.active_channels) {
                //console.log("active_channel: " + active_channel);

                active_channels.push(active_channel);

                spectra[active_channel] = _.find(message.data, v => (v.id === active_channel));
            }

            active_channels = _.sortBy(_.uniq(active_channels));
        }
    }

    function create_plot() {
        Plotly.newPlot('plot_spectrum', [{}, {}], layout_spectrum, config_spectrum);
    }

    function update_plot(force) {
        const force_update = !_.isNil(force);

        const now = dayjs();
        const difference = next_update_plot.diff(now, "seconds");

        if (difference <= 0 || force_update) {
            //console.log("Updating plot");

            const histo = spectra[selected_channel()].energy;

            const delta = (histo.config.max - histo.config.min) / histo.config.bins;

            const edges = histo.data.map(function (value, index) {
                return index * delta + histo.config.min;
            });

            const spectrum = {
                x: edges,
                y: histo.data,
                xaxis: 'x',
                yaxis: 'y',
                mode: 'lines',
                line: {shape: 'hv'},
                name: 'Spectrum',
                type: 'scatter'
            };

            const histo2D = spectra[selected_channel()].PSD;

            const delta_x = (histo2D.config.max_x - histo2D.config.min_x) / histo2D.config.bins_x;
            const delta_y = (histo2D.config.max_y - histo2D.config.min_y) / histo2D.config.bins_y;

            let edges_x = [];

            for (let index_x = 0; index_x < histo2D.config.bins_x; index_x++) {
                edges_x.push(index_x * delta_x + histo2D.config.min_x);
            }

            let edges_y = [];

            for (let index_y = 0; index_y < histo2D.config.bins_y; index_y++) {
                edges_y.push(index_y * delta_y + histo2D.config.min_y);
            }

            var heights = [];

            for (let index_y = 0; index_y < histo2D.config.bins_y; index_y++) {
                let row = [];

                for (let index_x = 0; index_x < histo2D.config.bins_x; index_x++) {
                    const index = (index_x + histo2D.config.bins_x * index_y);
                    const counts = Math.log10(histo2D.data[index]);

                    row.push(counts);
                }

                heights.push(row);
            }

            const PSD = {
                x: edges_x,
                y: edges_y,
                z: heights,
                xaxis: 'x',
                yaxis: 'y2',
                name: 'PSD vs E',
                colorscale: 'Viridis',
                showscale: false,
                type: 'heatmap'
            };

            const spectrum_data = [spectrum, PSD];

            Plotly.react('plot_spectrum', spectrum_data, layout_spectrum);

            const refresh_time = Number($("#time_refresh").val());
            next_update_plot = dayjs().add(refresh_time, "seconds");
        }
    }

    function on_data(message) {
        const decoded_string = utf8decoder.decode(message);
        const new_spectra = JSON.parse(decoded_string);
        add_to_spectra(new_spectra);
        update_selector(active_channels);
        update_plot();
    }

    function download_spectrum_data() {
        try {
            const this_selected_channel = selected_channel();
            const histo = spectra[this_selected_channel].energy;

            const delta = (histo.config.max - histo.config.min) / histo.config.bins;

            let csv_text = "#Spectrum for channel: " + this_selected_channel + " created on: " + dayjs().format() + "\r\n";

            csv_text += "#left_edge,counts\r\n";

            for (let index = 0; index < histo.data.length; index ++) {
                const edge = index * delta + histo.config.min;
                const counts = histo.data[index];

                csv_text += "" + edge + "," + counts + "\r\n";
            }
        
            create_and_download_file(csv_text, "Spectrum plot of channel " + this_selected_channel + ".csv", "txt");
        } catch (error) {
            console.error(error);
        }
    }

    function spec_arguments_reconfigure() {
        const channel_ids = $(".channel_ids").map(function (index, element) {
            return parseInt(this.value);
        }).get();
    
        const energy_channels = channel_ids.map(function (id) {
            return {"id": id,
                    "type": "energy",
                    "config": {
                        "verbosity": 0,
                        "bins": parseInt($("#energy_bins_" + id).val()),
                        "min": parseFloat($("#energy_min_" + id).val()),
                        "max": parseFloat($("#energy_max_" + id).val())
                    }
                   };
        });
    
        const PSD_channels = channel_ids.map(function (id) {
            return {"id": id,
                    "type": "PSD",
                    "config": {
                        "verbosity": 0,
                        "bins_x": parseInt($("#energy_bins_" + id).val()),
                        "min_x": parseFloat($("#energy_min_" + id).val()),
                        "max_x": parseFloat($("#energy_max_" + id).val()),
                        "bins_y": parseInt($("#PSD_bins_" + id).val()),
                        "min_y": parseFloat($("#PSD_min_" + id).val()),
                        "max_y": parseFloat($("#PSD_max_" + id).val())
                    }
                   };
        });
    
        const kwargs = {"channels": energy_channels.concat(PSD_channels)};
    
        return kwargs;
    }
    
    function spec_arguments_reset(channel) {
        return function () {
            const kwargs = {"channel": (_.isNil(channel) ? selected_channel() : channel),
                            "type": "all"};
    
            return kwargs;
        }
    }

    socket_io.on("connect", socket_io_connection(socket_io, module_name, on_status, update_events_log(), on_data));

    window.setInterval(function () {
        connection_checker.display();
    }, 1000);

    $("#channel_select").on('change', function () {
        console.log("Changed channel");
        update_plot(true);
    });

    $("#time_refresh").on('change', function () {
        const refresh_time = Number($("#time_refresh").val());
        next_update_plot = dayjs().add(refresh_time, "seconds");
        update_plot();
    });

    $("#button_download_spectrum_data").on("click", download_spectrum_data);
    $("#button_config_send").on("click", send_command(socket_io, 'reconfigure', spec_arguments_reconfigure));
    $("#button_reset_channel").on("click", send_command(socket_io, 'reset', spec_arguments_reset()));
    $("#button_reset_all").on("click", send_command(socket_io, 'reset', spec_arguments_reset("all")));

    // This part is to trigger a window resize to convince plotly to resize the plot
    var observer = new MutationObserver(function(mutations) {
        window.dispatchEvent(new Event('resize'));
    });
      
    observer.observe($("#plot_spectrum")[0], {attributes: true})

    // Then force a resize...
    $("#plot_spectrum").css("height", "" + default_plot_height + "px");

    create_plot();
}

$(page_loaded);