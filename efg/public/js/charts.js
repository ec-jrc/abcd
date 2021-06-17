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

function create_chart(context, yscale, lines_number, labels, colors, fills, stepped) {
    const chart_data = [{x: 0, y: 0}, {x: 1, y: 0}];

    var datasets = Array();

    for (var i = 0; i < lines_number; i++) {
        datasets[i] = {
            borderColor: colors[i],
            borderWidth: 2,
            pointRadius: 0,
            tension: 0,
            steppedLine: stepped[i],
            label: labels[i],
            data: chart_data
        }

        if (fills[i] === false) {
            datasets[i]["fill"] = false;
        } else {
            datasets[i]["fill"] = true;
            datasets[i]["backgroundColor"] = fills[i];
        }
    }

    return new Chart(context, {
        type: 'line',
        data: {
            datasets: datasets
        },
        options: {
            scales: {
                xAxes: [{
                    type: 'linear',
                    position: 'bottom'
                }],
                yAxes: [{
                    type: yscale
                }]
            }
        }
    }, {animation: false});
}

function generate_update_charts (type) {
    var old_data = {"timestamp": "###"};

    var selected_yscale = $("#spectrum_yscale").val();

    var qlong_chart_ctx = $("#" + type + "_chart");
    var qlong_chart = create_chart(qlong_chart_ctx, selected_yscale, 1, [type], ["blue"], ["rgba(0, 0, 255, 0.1)"], [false]);

    var last_update = moment();

    console.log("Created " + type + " chart, with " + selected_yscale + " y scale");

    return {
        'chart': qlong_chart,
        'update': function (new_data) {
            const update_period = parseInt($("#chart_refresh_time").val());
            const now = moment();

            //console.log("Updating plot? " + old_data["timestamp"] + " " + new_data["timestamp"]);
            if (new_data["timestamp"] !== old_data["timestamp"]
                &&
                now.diff(last_update, "seconds") > update_period) {

                //console.log("Updating plot " + new_data["timestamp"]);

                last_update = moment();
                old_data = new_data;

                var selected_channel = parseInt($("#channel_select").val());

                try {
                    const qlong_histo = new_data["data"][type][selected_channel];
                    const qlong_config = qlong_histo["config"];

                    const bins_qlong = qlong_config["bins"];
                    const min_qlong = qlong_config["min"];
                    const max_qlong = qlong_config["max"];
                    const bin_width_qlong = (max_qlong - min_qlong) / bins_qlong;

                    const y = qlong_histo["histo"];
                    const chart_data = y.map(function(val, index) {
                        return {x: index * bin_width_qlong + min_qlong, y: val};
                    });

                    qlong_chart.data.datasets[0].data = chart_data;

                    var y_scale_ID = qlong_chart.yAxisID;
                    //var y_scale = Chart.getScaleForId(y_scale_ID);
                    //y_scale.max = y.max();

                    qlong_chart.update();
                } catch (error) {}
            }
        }
    }
}

function generate_update_waveform () {
    const labels = ["Samples", "Gate 1", "Gate 2", "Gate 3", "Gate 4"];
    const colors = ["#4c72b0", "#55a868", "#c44e52", "#8172b2", "#ccb974"];
    var fills = [false, false, false, false, false];
    var stepped = [true, true, true, true, true];

    var waveform_chart_ctx = $("#waveform_chart");
    var waveform_chart = create_chart(waveform_chart_ctx, 'linear', 5, labels, colors, fills, stepped);

    var last_update = moment();

    return function (new_data) {
        const update_period = parseInt($("#chart_refresh_time").val());
        const now = moment();

        //console.log("Updating waveforms plot? Seconds since last update: " + now.diff(last_update, "seconds"));
        if (now.diff(last_update, "seconds") > update_period) {

            //console.log("Updating waveform plot");

            last_update = moment();

            var selected_channel = parseInt($("#waveforms_channel_select").val());

            try {
                const active_channels = new_data["active_channels"];
                const waveforms = new_data["samples"];

                var channel_select = $("#waveforms_channel_select").html('');

                active_channels.forEach(function (channel, index) {
                    if (channel === selected_channel) {
                        channel_select.append($('<option>', {value: channel, text: channel, selected: "selected"}));
                    } else {
                        channel_select.append($('<option>', {value: channel, text: channel}));
                    }
                });

                // Reread the selected channel after setting it, because on the
                // first update no channel will be selected by the for loop, but
                // the browser will do it automagically.
                selected_channel = parseInt($("#waveforms_channel_select").val());

                const w = waveforms[selected_channel];

                const chart_data = w.map(function(val, index) {
                    return {x: index, y: val};
                });

                waveform_chart.data.datasets[0].data = chart_data;

                const waveforms_zero = w[0];
                const waveforms_min = _.min(w) - 1;
                const waveforms_max = _.max(w);

                var chart_data_g = [{x: 0, y: waveforms_zero}];

                const gates = new_data["gates"];

                if (_.isNil(gates)) {
                    //console.log("Gates are NOT present");
                } else {
                    if (_.isEmpty(gates)) {
                        //console.log("Gates are present but empty");
                    } else {
                        //console.log("Gates are present");

                        // If the gates are not enabled the function should raise an exception
                        // and stop here.
                        for (var gate_index = 0; gate_index < 4; gate_index++) {
                            const gate_samples = gates[selected_channel][gate_index];

                            if (_.isNil(gate_samples)) {
                                //console.log("Gates are present but the channel " + gate_index + " is empty");
                                waveform_chart.data.datasets[1 + gate_index].data = chart_data_g;
                            } else {
                                chart_data_gs = gate_samples.map(function(val, index) {
                                    //return {x: index, y: (val * (0.7 - gate_index * 0.1) * (waveforms_max - waveforms_min)) + waveforms_min};
                                    //return {x: index, y: val};
                                    //return {x: index, y: (val / 255 * (waveforms_max - waveforms_min)) + waveforms_min / 4};
                                    //return {x: index, y: (val / 255 * (1 << 14)) + waveforms_min / 2};
                                    return {x: index, y: (val / 255 * (waveforms_max - waveforms_min)) - waveforms_max + 2 * waveforms_min};
                                });

                                waveform_chart.data.datasets[1 + gate_index].data = chart_data_gs;
                            }
                        }
                    }
                }
            } catch (error) {}

            waveform_chart.update();
        }
    }
}
