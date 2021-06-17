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

    var histogram_painter = new HistogramPainter("#Spectrum_plot", HISTOGRAM_WIDTH, HISTOGRAM_HEIGHT);
    var histogram2D_painter = new Histogram2DPainter("#PSD_plot", HISTOGRAM_HEIGHT, HISTOGRAM_HEIGHT);

    $("#plot_download_image").click(function () {histogram_painter.download_image();});
    $("#plot_download_data").click(function () {histogram_painter.download_data();});
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
            const selected_yscale = $("#spectrum_yscale").val();
            const selected_colors_scale = $("#PSD_colors_scale").val();
            console.log("Selected channel: " + selected_channel + "; scale: " + selected_yscale);

            try {
                const channels = old_data["data"];

                const channel = channels.find(function (channel) {
                    return channel.id === selected_channel;
                });

                histogram_painter.update_display(channel.energy, selected_yscale);
                histogram2D_painter.update_display(channel.PSD, selected_colors_scale);
            } catch (error) {console.error(error);}
        }
    }
}
