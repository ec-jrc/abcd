// (C) Copyright 2016,2021,2023 European Union, Cristiano Lino Fontana
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
            y: 1.02,
            yanchor: 'bottom'
        }
    ]

    var layout_spectrum = {
        xaxis: {
            title: 'Energy [ch]',
            autorange: true,
            autotick: true,
            showspikes: true,
            spikemode: 'across',
            domain: [0.0, 1.0],
            anchor: 'y2'
        },
        yaxis: {
            title: 'Counts',
            autorange: true,
            autotick: true,
            showspikes: true,
            spikemode: 'across',
            domain: [0.5, 1.0]
        },
        yaxis2: {
            title: 'PSD parameter',
            autorange: true,
            autotick: true,
            showspikes: true,
            spikemode: 'across',
            domain: [0.0, 0.5]
        },
        updatemenus: updatemenus_spectrum,
        margin: {
           t: 10,
           l: 70,
           r: 10
        },
        //showlegend: true,
        legend: {
            x: 1,
            xanchor: 'right',
            y: 1,
            yanchor: 'top',
        },
        grid: {
            rows: 2,
            columns: 1,
            roworder:'top to bottom'
        },
        hovermode: 'closest'
    };

    const config_spectrum = {
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
    var last_spec_config = null;

    var spec_config_editor = ace.edit("online_editor");
    spec_config_editor.setTheme("ace/theme/github");
    spec_config_editor.setShowPrintMargin(false);
    spec_config_editor.setOptions({"fontFamily": '"Fira Mono"'});

    spec_config_editor.getSession().setMode("ace/mode/json");
    spec_config_editor.getSession().setTabSize(4);
    spec_config_editor.getSession().setUseSoftTabs(true);

    spec_config_editor.container.style.lineHeight = 2;
    spec_config_editor.renderer.updateFontSize();
    spec_config_editor.resize();

    function on_status(message) {
        const decoded_string = utf8decoder.decode(message);
        const new_status = JSON.parse(decoded_string);

        connection_checker.beat();

        if (new_status["timestamp"] !== old_status["timestamp"])
        {
            old_status = new_status;

            //console.log("Updating Speccalc status");

            if (new_status.hasOwnProperty("config")) {
                last_spec_config = new_status["config"];
            }

            const this_selected_channel = selected_channel();

            //console.log("Selected channel: " + this_selected_channel);
            //console.log("New status: " + JSON.stringify(new_status));
            //console.log("Channels: " + new_status["active_channels"]);
            try {
                const active_channels = new_status["active_channels"];
                update_selector(active_channels);
            } catch (error) { }

            if (new_status.hasOwnProperty("statuses")) {
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
        Plotly.newPlot('plot_spectrum', [{}, {}], layout_spectrum, config_spectrum);
    }

    function update_plot(force) {
        const force_update = !_.isNil(force);

        const now = dayjs();
        const difference = next_update_plot.diff(now, "seconds");

        if (difference <= 0 || force_update) {
            //console.log("Updating plot");

            const histo_energy = spectra[selected_channel()].energy;

            const delta_energy = (histo_energy.config.max - histo_energy.config.min) / histo_energy.config.bins;

            const edges_energy = histo_energy.data.map(function (value, index) {
                return index * delta_energy + histo_energy.config.min;
            });

            const energy = {
                x: edges_energy,
                y: histo_energy.data,
                xaxis: 'x',
                yaxis: 'y',
                mode: 'lines',
                line: {shape: 'hv'},
                name: 'Spectrum (uncalibrated)',
                type: 'scatter'
            };

            const spectrum_data = [energy];
            let PSD_data = [];

            if (spectra[selected_channel()].hasOwnProperty("PSD")) {
                const histo_EvsPSD = spectra[selected_channel()].PSD;

                const delta_x = (histo_EvsPSD.config.max_x - histo_EvsPSD.config.min_x) / histo_EvsPSD.config.bins_x;
                const delta_y = (histo_EvsPSD.config.max_y - histo_EvsPSD.config.min_y) / histo_EvsPSD.config.bins_y;

                let edges_x = [];

                for (let index_x = 0; index_x < histo_EvsPSD.config.bins_x; index_x++) {
                    edges_x.push(index_x * delta_x + histo_EvsPSD.config.min_x);
                }

                let edges_y = [];

                for (let index_y = 0; index_y < histo_EvsPSD.config.bins_y; index_y++) {
                    edges_y.push(index_y * delta_y + histo_EvsPSD.config.min_y);
                }

                var heights = [];

                for (let index_y = 0; index_y < histo_EvsPSD.config.bins_y; index_y++) {
                    let row = [];

                    for (let index_x = 0; index_x < histo_EvsPSD.config.bins_x; index_x++) {
                        const index = (index_x + histo_EvsPSD.config.bins_x * index_y);
                        const counts = Math.log10(histo_EvsPSD.data[index]);

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

                PSD_data = [PSD];
            }

            if (force_update) {
                // If forced, plotting without the fits so then the
                // data would be ready for the fit
                Plotly.react('plot_spectrum', spectrum_data.concat(PSD_data), layout_spectrum);
            }

            let fitter = fitters[selected_channel()];
            
            fitter.fit_all();

            const other_data = fitter.get_all_plots();

            Plotly.react('plot_spectrum', spectrum_data.concat(other_data).concat(PSD_data), layout_spectrum);

            $("#fits_results").empty().append(fitter.get_html_all_fit_results());

            let graph_div = document.getElementById('plot_spectrum')
            const data_index = 0;
            const range = graph_div.layout.xaxis.range;

            const current_region = fitter.get_region(graph_div, data_index, range);
            
            $("#region_statistics").empty().append(fitter.get_html_statistics(current_region));

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

    function fit_energy() {
        if (!_.isNil(fitters[selected_channel()])) {
            let graph_div = document.getElementById('plot_spectrum')
            const data_index = 0;
            const range = graph_div.layout.xaxis.range;

            fitters[selected_channel()].add_fit(graph_div, data_index, range);

            update_plot(true);
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
        
            const file_name = "Spectrum plot of channel " + this_selected_channel + " at " + dayjs().format("YYYY-MM-DDTHH.mm.ss") + ".csv";
        
            create_and_download_file(csv_text, file_name, "txt");
        } catch (error) {
            console.error(error);
        }
    }

    function spec_arguments_reconfigure() {
        try {
            const new_text_config = spec_config_editor.getSession().getValue();
            const new_config = JSON.parse(new_text_config);
            const kwargs = {"config": new_config};

            return kwargs;

        } catch (error) {
            console.log("Error: " + error);

            return null;
        }
    }
    
    function spec_get_config() {
        if (_.isNil(last_spec_config)) {
            spec_config_editor.getSession().setValue(JSON.stringify({"error": "Unable to load spec config"}, null, 4));
            spec_config_editor.gotoLine(0);
        } else {
            spec_config_editor.getSession().setValue(JSON.stringify(last_spec_config, null, 4));
            spec_config_editor.gotoLine(0);
        }
    }

    function spec_download_config() {
        const new_text_config = spec_config_editor.getSession().getValue();
        const file_name = "spec_config_" + dayjs().format("YYYY-MM-DDTHH.mm.ss") + ".json";

        //console.log("Preparing file with name: " + file_name);

        create_and_download_file(new_text_config, file_name, "txt");
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

    $("#button_fit_energy").on("click", fit_energy);
    $("#button_clear_fit").on("click", function () {
        if (!_.isNil(fitters[selected_channel()])) {
            fitters[selected_channel()].clear();
            update_plot(true);
        }
    });
    $("#button_download_spectrum_data").on("click", download_spectrum_data);
    $("#button_config_send").on("click", send_command(socket_io, 'reconfigure', spec_arguments_reconfigure));
    $("#button_config_get").on("click", spec_get_config);
    $("#button_config_download").on("click", spec_download_config);
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
