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

"use strict";

function generate_update_plot() {
    var old_data = {"timestamp": "###"};

    const HISTOGRAM_WIDTH = 1024;
    const HISTOGRAM_HEIGHT = HISTOGRAM_WIDTH / 16 * 9;

    var histogram_ToF_painter = new HistogramPainter("#ToF_plot", HISTOGRAM_WIDTH, HISTOGRAM_HEIGHT);
    var histogram_E_painter = new HistogramPainter("#E_plot", HISTOGRAM_WIDTH, HISTOGRAM_HEIGHT);
    var histogram2D_painter = new Histogram2DPainter("#EToF_plot", HISTOGRAM_HEIGHT, HISTOGRAM_HEIGHT);

    $("#ToF_plot_download_image").click(function () {histogram_ToF_painter.download_image();});
    $("#ToF_plot_download_data").click(function () {histogram_ToF_painter.download_data();});
    $("#E_plot_download_image").click(function () {histogram_E_painter.download_image();});
    $("#E_plot_download_data").click(function () {histogram_E_painter.download_data();});
    $("#plot2D_download_image").click(function () {histogram2D_painter.download_image();});

    return function (new_data) {
        const now = moment();
        const difference = next_update_plot.diff(now, "seconds");

        if (difference <= 0) {
            console.log("Updating plot");

            const update_period = parseInt($("#plot_refresh_time").val());

            next_update_plot = now.add(update_period, "seconds");

            if (!_.isNil(new_data)) {
                old_data = new_data;
            }

            const selected_channel = parseInt($("#channel_select").val());
            const selected_ToF_yscale = $("#ToF_yscale").val();
            const selected_E_yscale = $("#spectrum_yscale").val();
            const selected_colors_scale = $("#EToF_colors_scale").val();
            console.log("Selected channel: " + selected_channel + "; ToF scale: " + selected_ToF_yscale + "; E scale: " + selected_E_yscale);

            try {
                const channels = old_data["data"];

                const channel = channels.find(function (channel) {
                    return channel.id === selected_channel;
                });

                histogram_ToF_painter.update_display(channel.ToF, selected_ToF_yscale);
                histogram_E_painter.update_display(channel.energy, selected_E_yscale);
                histogram2D_painter.update_display(channel.EToF, selected_colors_scale);
            } catch (error) {console.error(error);}
        }
    }
}
