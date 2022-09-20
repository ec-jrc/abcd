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

    const default_time_refresh = 5;
    const default_plot_height = 900;

    var connection_checker = new ConnectionChecker();
    var fitters = {};

    var updatemenus_ToF = [
        {
            buttons: [
                {
                    args: ['yaxis.type', 'lin'],
                    label:'ToF Linear Y',
                    method:'relayout'
                },
                {
                    args: ['yaxis.type', 'log'],
                    label: 'ToF Logarithmic Y',
                    method: 'relayout'
                }
            ],
            direction: 'down',
            pad: {'r': 10, 't': 10},
            showactive: true,
            type: 'dropdown',
            x: 0.0,
            xanchor: 'left',
            y: 1.02,
            yanchor: 'bottom'
        },
        {
            buttons: [
                {
                    args: ['yaxis3.type', 'lin'],
                    label:'En. Linear Y',
                    method:'relayout'
                },
                {
                    args: ['yaxis3.type', 'log'],
                    label: 'En. Logarithmic Y',
                    method: 'relayout'
                }
            ],
            direction: 'down',
            pad: {'r': 10, 't': 10},
            showactive: true,
            type: 'dropdown',
            x: 0.0,
            xanchor: 'left',
            y: 0.42,
            yanchor: 'bottom'
        }
    ]

    var layout_ToF = {
        xaxis: {
            title: 'ToF [ns]',
            autorange: true,
            autotick: true,
            showspikes: true,
            spikemode: 'across',
            domain: [0.0, 1.0],
            anchor: 'y2'
        },
        xaxis2: {
            title: 'Energy [ch]',
            autorange: true,
            autotick: true,
            showspikes: true,
            spikemode: 'across',
            domain: [0.0, 1.0],
            anchor: 'y4'
        },
        yaxis: {
            title: 'Counts',
            autorange: true,
            autotick: true,
            showspikes: true,
            spikemode: 'across',
            domain: [0.75, 1.0]
            //anchor: 'y1'
        },
        yaxis2: {
            title: 'Energy [ch]',
            autorange: true,
            autotick: true,
            showspikes: true,
            spikemode: 'across',
            domain: [0.5, 0.75]
            //anchor: 'y2'
        },
        yaxis3: {
            title: 'Counts',
            autorange: true,
            autotick: true,
            showspikes: true,
            spikemode: 'across',
            domain: [0.25, 0.4]
            //anchor: 'y3'
        },
        yaxis4: {
            title: 'Energy of reference [ch]',
            autorange: true,
            autotick: true,
            showspikes: true,
            spikemode: 'across',
            domain: [0.0, 0.25]
            //anchor: 'y3'
        },
        updatemenus: updatemenus_ToF,
        margin: {
           t: 10,
           l: 70,
           r: 10
        },
        showlegend: true,
        legend: {
            x: 1,
            xanchor: 'right',
            y: 1,
            yanchor: 'top',
        },
        grid: {
            rows: 4,
            columns: 1,
            //pattern: 'independent',
            roworder:'top to bottom'
        },
        hovermode: 'closest'
    };

    const config_ToF = {
        responsive: true,
        modeBarButtonsToRemove: ['lasso2d']
    }

    var socket_io = io();

    var next_update_plot = dayjs().add(-10, "seconds");

    var active_channels = [];
    var spectra = {};

    const module_name = String($('input#module_name').val());
    
    console.log("Module name: " + module_name);

    $("#time_refresh").val(default_time_refresh);

    var old_status = {"timestamp": "###"};
    var last_tofcalc_config = null;

    var tofcalc_config_editor = ace.edit("online_editor");
    tofcalc_config_editor.setTheme("ace/theme/github");
    tofcalc_config_editor.setShowPrintMargin(false);
    tofcalc_config_editor.setOptions({"fontFamily": '"Fira Mono"'});

    tofcalc_config_editor.getSession().setMode("ace/mode/json");
    tofcalc_config_editor.getSession().setTabSize(4);
    tofcalc_config_editor.getSession().setUseSoftTabs(true);

    tofcalc_config_editor.container.style.lineHeight = 2;
    tofcalc_config_editor.renderer.updateFontSize();
    tofcalc_config_editor.resize();

    function on_status(message) {
        const decoded_string = utf8decoder.decode(message);
        const new_status = JSON.parse(decoded_string);

        connection_checker.beat();

        if (new_status["timestamp"] !== old_status["timestamp"])
        {
            old_status = new_status;

            if (new_status.hasOwnProperty("config")) {
                last_tofcalc_config = new_status["config"];
            }

            const this_selected_channel = selected_channel();

            //console.log("Selected channel: " + this_selected_channel);
            //console.log("New status: " + JSON.stringify(new_status));
            //console.log("Channels: " + new_status["active_channels"]);
            try {
                const active_channels = new_status["active_channels"];
                update_selector(active_channels);
            } catch (error) { }

            const new_channels_statuses = new_status["statuses"];

            let rates_list = $("<ul>");

            new_channels_statuses.forEach(function (channel_status) {
                const channel = channel_status["id"];
                const rate = channel_status["rate"];

                rates_list.append($("<li>", {text: "Ch " + channel + ": " + rate.toFixed(2)}));
            });

            $("#channels_rates").empty().append(rates_list);

            let counts_list = $("<ul>");

            new_channels_statuses.forEach(function (channel_status) {
                const channel = channel_status["id"];
                const counts = channel_status["counts"];

                counts_list.append($("<li>", {text: "Ch " + channel + ": " + counts.toFixed(0)}));
            });

            $("#channels_counts").empty().append(counts_list);
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

                // Creating a fitter here to be sure that there already
                // are plots available to fit.
                if (_.isNil(fitters[active_channel])) {
                    fitters[active_channel] = new Fitter();
                }
            }

            active_channels = _.sortBy(_.uniq(active_channels));
        }
    }

    function create_plot() {
        Plotly.newPlot('plot_ToF', [{}, {}, {}, {}], layout_ToF, config_ToF);
    }

    function update_plot(force) {
        const force_update = !_.isNil(force);

        const now = dayjs();
        const difference = next_update_plot.diff(now, "seconds");

        if (difference <= 0 || force_update) {
            //console.log("Updating plot");

            const histo_ToF = spectra[selected_channel()].ToF;

            const delta_ToF = (histo_ToF.config.max - histo_ToF.config.min) / histo_ToF.config.bins;

            const edges_ToF = histo_ToF.data.map(function (value, index) {
                return index * delta_ToF + histo_ToF.config.min;
            });

            const ToF = {
                x: edges_ToF,
                y: histo_ToF.data,
                xaxis: 'x1',
                yaxis: 'y1',
                mode: 'lines',
                line: {shape: 'hv'},
                name: 'Time-of-Flight (calibrated)',
                type: 'scattergl',
                mode: 'lines',
                marker: {
                    size: 0.1
                }
            };

            const histo_energy = spectra[selected_channel()].energy;

            const delta_energy = (histo_energy.config.max - histo_energy.config.min) / histo_energy.config.bins;

            const edges_energy = histo_energy.data.map(function (value, index) {
                return index * delta_energy + histo_energy.config.min;
            });

            const energy = {
                x: edges_energy,
                y: histo_energy.data,
                xaxis: 'x2',
                yaxis: 'y3',
                mode: 'lines',
                line: {shape: 'hv'},
                name: 'Spectrum (uncalibrated)',
                type: 'scattergl',
                mode: 'lines',
                marker: {
                    size: 0.1
                }
            };

            const histo_EvsToF = spectra[selected_channel()].EvsToF;

            const EvsToF_delta_x = (histo_EvsToF.config.max_x - histo_EvsToF.config.min_x) / histo_EvsToF.config.bins_x;
            const EvsToF_delta_y = (histo_EvsToF.config.max_y - histo_EvsToF.config.min_y) / histo_EvsToF.config.bins_y;

            let EvsToF_edges_x = [];

            for (let index_x = 0; index_x < histo_EvsToF.config.bins_x; index_x++) {
                EvsToF_edges_x.push(index_x * EvsToF_delta_x + histo_EvsToF.config.min_x);
            }

            let EvsToF_edges_y = [];

            for (let index_y = 0; index_y < histo_EvsToF.config.bins_y; index_y++) {
                EvsToF_edges_y.push(index_y * EvsToF_delta_y + histo_EvsToF.config.min_y);
            }

            var EvsToF_heights = [];

            for (let index_y = 0; index_y < histo_EvsToF.config.bins_y; index_y++) {
                let EvsToF_row = [];

                for (let index_x = 0; index_x < histo_EvsToF.config.bins_x; index_x++) {
                    const index = (index_x + histo_EvsToF.config.bins_x * index_y);
                    const counts = Math.log10(histo_EvsToF.data[index]);

                    EvsToF_row.push(counts);
                }

                EvsToF_heights.push(EvsToF_row);
            }

            //console.log("Updated EvsToF; edges x: " + EvsToF_edges_x.length + "; edges y: " + EvsToF_edges_y.length + ";");

            const EvsToF = {
                x: EvsToF_edges_x,
                y: EvsToF_edges_y,
                z: EvsToF_heights,
                xaxis: 'x1',
                yaxis: 'y2',
                name: 'E vs ToF',
                colorscale: 'Viridis',
                showscale: false,
                type: 'heatmap'
            };

            const histo_EvsE = spectra[selected_channel()].EvsE;

            const EvsE_delta_x = (histo_EvsE.config.max_x - histo_EvsE.config.min_x) / histo_EvsE.config.bins_x;
            const EvsE_delta_y = (histo_EvsE.config.max_y - histo_EvsE.config.min_y) / histo_EvsE.config.bins_y;

            let EvsE_edges_x = [];

            for (let index_x = 0; index_x < histo_EvsE.config.bins_x; index_x++) {
                EvsE_edges_x.push(index_x * EvsE_delta_x + histo_EvsE.config.min_x);
            }

            let EvsE_edges_y = [];

            for (let index_y = 0; index_y < histo_EvsE.config.bins_y; index_y++) {
                EvsE_edges_y.push(index_y * EvsE_delta_y + histo_EvsE.config.min_y);
            }

            var EvsE_heights = [];

            for (let index_y = 0; index_y < histo_EvsE.config.bins_y; index_y++) {
                let EvsE_row = [];

                for (let index_x = 0; index_x < histo_EvsE.config.bins_x; index_x++) {
                    const index = (index_x + histo_EvsE.config.bins_x * index_y);
                    const counts = Math.log10(histo_EvsE.data[index]);

                    EvsE_row.push(counts);
                }

                EvsE_heights.push(EvsE_row);
            }

            //console.log("Updated EvsE; edges x: " + EvsE_edges_x.length + "; edges y: " + EvsE_edges_y.length + ";");

            const EvsE = {
                x: EvsE_edges_x,
                y: EvsE_edges_y,
                z: EvsE_heights,
                xaxis: 'x2',
                yaxis: 'y4',
                name: 'E vs E',
                colorscale: 'Viridis',
                showscale: false,
                type: 'heatmap'
            };

            const tofcalc_data = [ToF, energy];
            const bidimensional_data = [EvsToF, EvsE];

            if (force_update) {
                // If forced, plotting without the fits so then the
                // data would be ready for the fit
                Plotly.react('plot_ToF', tofcalc_data.concat(bidimensional_data), layout_ToF);
            }

            let fitter = fitters[selected_channel()];
            
            fitter.fit_all();

            const other_data = fitter.get_all_plots();

            Plotly.react('plot_ToF', tofcalc_data.concat(other_data).concat(bidimensional_data), layout_ToF);

            $("#fits_results").empty().append(fitter.get_html_ol());

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

    function fit_ToF() {
        if (!_.isNil(fitters[selected_channel()])) {
            let graph_div = document.getElementById('plot_ToF')
            const data_index = 0;
            const range = graph_div.layout.xaxis.range;

            fitters[selected_channel()].add_fit(graph_div, data_index, range);

            update_plot(true);
        }
    }

    function fit_energy() {
        if (!_.isNil(fitters[selected_channel()])) {
            let graph_div = document.getElementById('plot_ToF')
            const data_index = 1;
            const range = graph_div.layout.xaxis2.range;

            fitters[selected_channel()].add_fit(graph_div, data_index, range);

            update_plot(true);
        }
    }

    function download_ToF_data() {
        try {
            const this_selected_channel = selected_channel();
            const histo_ToF = spectra[this_selected_channel].ToF;

            const delta = (histo_ToF.config.max - histo_ToF.config.min) / histo_ToF.config.bins;

            let csv_text = "#ToF distribution for channel: " + this_selected_channel + " created on: " + dayjs().format() + "\r\n";

            csv_text += "#left_edge [ns],counts\r\n";

            for (let index = 0; index < histo_ToF.data.length; index ++) {
                const edge = index * delta + histo_ToF.config.min;
                const counts = histo_ToF.data[index];

                csv_text += "" + edge + "," + counts + "\r\n";
            }
        
            create_and_download_file(csv_text, "ToF plot of channel " + this_selected_channel + ".csv", "txt");
        } catch (error) {
            console.error(error);
        }
    }

    function download_spectrum_data() {
        try {
            const this_selected_channel = selected_channel();
            const histo_energy = spectra[this_selected_channel].energy;

            const delta = (histo_energy.config.max - histo_energy.config.min) / histo_energy.config.bins;

            let csv_text = "#Spectrum for channel: " + this_selected_channel + " created on: " + dayjs().format() + "\r\n";

            csv_text += "#left_edge [ch],counts\r\n";

            for (let index = 0; index < histo_energy.data.length; index ++) {
                const edge = index * delta + histo_energy.config.min;
                const counts = histo_energy.data[index];

                csv_text += "" + edge + "," + counts + "\r\n";
            }
        
            create_and_download_file(csv_text, "Spectrum plot of channel " + this_selected_channel + ".csv", "txt");
        } catch (error) {
            console.error(error);
        }
    }

    function tofcalc_arguments_config() {
        try {
            const new_text_config = tofcalc_config_editor.getSession().getValue();
            const new_config = JSON.parse(new_text_config);
            const kwargs = {"config": new_config};

            return kwargs;

        } catch (error) {
            console.log("Error: " + error);

            return null;
        }
    }

    function tofcalc_get_config() {
        if (_.isNil(last_tofcalc_config)) {
            tofcalc_config_editor.getSession().setValue(JSON.stringify({"error": "Unable to load tofcalc config"}, null, 4));
            tofcalc_config_editor.gotoLine(0);
        } else {
            tofcalc_config_editor.getSession().setValue(JSON.stringify(last_tofcalc_config, null, 4));
            tofcalc_config_editor.gotoLine(0);
        }
    }

    function tofcalc_download_config() {
        const new_text_config = tofcalc_config_editor.getSession().getValue();
        const file_name = "tofcalc_config_" + dayjs().format("YYYY-MM-DDTHH.mm.ss") + ".json";

        //console.log("Preparing file with name: " + file_name);

        create_and_download_file(new_text_config, file_name, "txt");
    }

    function tofcalc_arguments_reset(channel) {
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

    $("#button_fit_ToF").on("click", fit_ToF);
    $("#button_fit_energy").on("click", fit_energy);
    $("#button_clear_fit").on("click", function () {
        if (!_.isNil(fitters[selected_channel()])) {
            fitters[selected_channel()].clear();
            update_plot(true);
        }
    });
    $("#button_download_ToF_data").on("click", download_ToF_data);
    $("#button_download_spectrum_data").on("click", download_spectrum_data);
    $("#button_config_send").on("click", send_command(socket_io, 'reconfigure', tofcalc_arguments_config));
    $("#button_config_get").on("click", tofcalc_get_config);
    $("#button_config_download").on("click", tofcalc_download_config);
    $("#button_reset_channel").on("click", send_command(socket_io, 'reset', tofcalc_arguments_reset()));
    $("#button_reset_all").on("click", send_command(socket_io, 'reset', tofcalc_arguments_reset("all")));

    // This part is to trigger a window resize to convince plotly to resize the plot
    var observer = new MutationObserver(function(mutations) {
        window.dispatchEvent(new Event('resize'));
    });
      
    observer.observe($("#plot_ToF")[0], {attributes: true});

    // Then force a resize...
    $("#plot_ToF").css("height", "" + default_plot_height + "px");

    create_plot();

    var graph_div = document.getElementById('plot_ToF');
    
    graph_div.on('plotly_relayout', function(eventdata){
        if (Object.keys(eventdata).includes("xaxis.range[0]")) {
            console.log("Zoom or pan on ToF or EvsToF");

            const update = {
                "xaxis2.range[0]": eventdata["yaxis2.range[0]"],
                "xaxis2.range[1]": eventdata["yaxis2.range[1]"]
            }
            Plotly.relayout("plot_ToF", update);
        } else if (Object.keys(eventdata).includes("xaxis2.range[0]")) {
            console.log("Zoom or pan on energy");

            const update = {
                "yaxis2.range[0]": eventdata["xaxis2.range[0]"],
                "yaxis2.range[1]": eventdata["xaxis2.range[1]"]
            }
            Plotly.relayout("plot_ToF", update);
        } else if (Object.keys(eventdata).includes("xaxis.autorange")) {
            console.log("Autoscaling plots, doing nothing")
        }
    });

}

$(page_loaded);
