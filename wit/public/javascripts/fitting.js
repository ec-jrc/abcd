// (C) Copyright 2016,2021,2025 European Union, Cristiano Lino Fontana
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

class Fitter {
    constructor () {
        this.fitting_regions = [];
    }

    get_region(graph_div, data_index, range) {
        // We are also reading the range object and not only the graph_div
        // reference because the range could change depending on the user choice
        return {
            "graph_div": graph_div,
            "data_index": data_index,
            "xaxis": graph_div.data[data_index].xaxis,
            "yaxis": graph_div.data[data_index].yaxis,
            "range": range,
            "N": 100,
            "last_params": null
        };
    }

    add_fit(graph_div, data_index, range) {
        this.fitting_regions.push(this.get_region(graph_div, data_index, range));
    }

    get_selected_points(fitting_region) {
        const xs = fitting_region.graph_div.data[fitting_region.data_index].x;
        const ys = fitting_region.graph_div.data[fitting_region.data_index].y;

        const N = Math.min(xs.length, ys.length);

        let sum_y = 0;

        let selected_xs = [];
        let selected_ys = [];

        for (let i = 0; i < N; i++) {
            if (fitting_region.range[0] < xs[i] && xs[i] < fitting_region.range[1]) {
                selected_xs.push(xs[i]);
                selected_ys.push(ys[i]);

                sum_y += ys[i];
            }
        }

        return {"xs": selected_xs, "ys": selected_ys, "sum_y": sum_y};
    }

    estimate_params(xs, ys) {
        const N = Math.min(xs.length, ys.length);

        const x_i = xs[0];
        const x_f = xs[N - 1];

        const y_i = ys[0];
        const y_f = ys[N - 1];

        const m = (y_f - y_i) / (x_f - x_i);
        const q = y_i;

        let y_cleans = [];
        let y_cleans_max = 0.0;
        let centroid = 0;
        let y_cleans_total = 0;

        for (let i = 0; i < N; i++) {
            const clean = ys[i] - m * (xs[i] - x_i) - q;

            centroid += xs[i] * clean;
            y_cleans_total += clean;

            y_cleans.push(clean);

            if (y_cleans_max < clean) {
                y_cleans_max = clean;
            }
        }

        centroid /= y_cleans_total;

        let variance = 0.0;

        for (let i = 0; i < N; i++) {
            variance += (xs[i] - centroid) * (xs[i] - centroid) * y_cleans[i]
        }

        variance /= y_cleans_total;

        // Using the abs() for pathological cases in which the
        // background is worngly estimated
        const stddev = Math.sqrt(Math.abs(variance));

        return [m, q - m * x_i, y_cleans_max, centroid, stddev];
    }

    peak(x, params) {
        const m = params[0];
        const q = params[1];
        const A = params[2];
        const mu = params[3];
        const sigma = params[4];

        return m * x + q + A * Math.exp(-0.5 * (x - mu) * (x - mu) / (sigma * sigma));
    }

    peak_gradient(x, params) {
        const m = params[0];
        const q = params[1];
        const A = params[2];
        const mu = params[3];
        const sigma = params[4];

        return [
            x,
            1,
            Math.exp(-0.5 * (x - mu) * (x - mu) / (sigma * sigma)),
            A * (x - mu) * Math.exp(-0.5 * (x - mu) * (x - mu) / (sigma * sigma)) / (sigma * sigma),
            A * (x - mu) * (x - mu) * Math.exp(-0.5 * (x - mu) * (x - mu) / (sigma * sigma)) / (sigma * sigma * sigma)
        ]
    }

    residuals(xs, ys, params) {
        const N = Math.min(xs.length, ys.length);

        let residuals_sum = 0;

        for (let i = 0; i < N; i++) {
            const residual = ys[i] - this.peak(xs[i], params);
            residuals_sum += Number(residual * residual);
        }

        return residuals_sum;
    }

    fit_region(fitting_region) {
        const data = this.get_selected_points(fitting_region);

        fitting_region.N = data.xs.length;

        if (data.xs.length > 2) {
            fitting_region.bin_width = (data.xs[1] - data.xs[0]);
        } else {
            fitting_region.bin_width = null;
        }

        let starting_params = fitting_region.last_params;

        if (_.isNil(fitting_region.last_params)) {
            starting_params = this.estimate_params(data.xs, data.ys);
            
            console.log("First estimation: " + 
                "m: " + starting_params[0] + " " +
                "q: " + starting_params[1] + " " +
                "A: " + starting_params[2] + " " +
                "mu: " + starting_params[3] + " " +
                "sigma: " + starting_params[4]);
        }

        let counter_calls = 0;

        const solution = fmin.nelderMead(params => {counter_calls += 1; return this.residuals(data.xs, data.ys, params)}, starting_params);

        fitting_region.last_params = solution.x;
        fitting_region.sum_y = data.sum_y;

        return solution;
    }

    fit_all() {
        for (let i = 0; i < this.fitting_regions.length; i++) {
            this.fit_region(this.fitting_regions[i]);
        }
    }

    get_plot(fitting_region) {
        let xs = [];
        let ys = [];

        const x_min = fitting_region.range[0];
        const x_max = fitting_region.range[1];

        const delta = (x_max - x_min) / fitting_region.N;

        for (let i = 0; i < fitting_region.N; i++) {
            const x = i * delta + x_min;
            const y = this.peak(x, fitting_region.last_params);

            // Adding a 0.5 to display the fit aligned with the bin center and not the edge
            xs.push(x + 0.5 * delta);
            ys.push(y);
        }

        return {
            x: xs,
            y: ys,
            xaxis: fitting_region.xaxis,
            yaxis: fitting_region.yaxis,
            name: 'Fit',
            type: 'scatter',
            mode: 'lines'
        };
    }

    get_all_plots() {
        let plots = [];

        for (let i = 0; i < this.fitting_regions.length; i++) {
            let plot = this.get_plot(this.fitting_regions[i])
            plot.name = 'Fit n.' + (i + 1);

            plots.push(plot);
        }

        return plots;
    }

    get_params(fitting_region) {
        return {"params": fitting_region.last_params,
            "m": fitting_region.last_params[0],
            "q": fitting_region.last_params[1],
            "A": fitting_region.last_params[2],
            "mu": fitting_region.last_params[3],
            "sigma": fitting_region.last_params[4],
            "range": fitting_region.range,
            "sum_y": fitting_region.sum_y,
            "bin_width": fitting_region.bin_width
        }
    }

    get_all_params() {
        let params = [];

        for (let i = 0; i < this.fitting_regions.length; i++) {
            let p = this.get_params(this.fitting_regions[i]);
            p.data_index = i;

            params.push(p);
        }

        return params;
    }

    get_html_statistics(current_region) {
        const data = this.get_selected_points(current_region);

        let statistics = $("<ul>");

        statistics.append($("<li>", {text: `Total counts in range: ${data.sum_y.toFixed(0)} counts`}));

        return statistics;
    }

    get_html_all_fit_results() {
        let fits_results = $("<ol>");

        this.get_all_params().forEach(function (p) {
            let fit = $("<ul>");

            const integral = Math.sqrt(2 * Math.PI) * p.sigma * p.A;
            const FWHM = 2 * Math.sqrt(2 * Math.log(2)) * p.sigma;
            const counts_in_peak = integral / p.bin_width;

            fit.append($("<li>", {text: "Fit range: [" + p.range[0].toFixed(2) + " ch, " + p.range[1].toFixed(2) + " ch]"}));
            fit.append($("<li>", {text: "Lin. background slope: " + p.m.toFixed(4) + " counts/ch"}));
            fit.append($("<li>", {text: "Lin. background intercept: " + p.q.toFixed(2) + " counts"}));
            fit.append($("<li>", {text: "Gaussian height: " + p.A.toFixed(2) + " counts"}));
            fit.append($("<li>", {text: "Gaussian center: " + p.mu.toFixed(2) + " ch"}));
            fit.append($("<li>", {text: "Gaussian sigma: " + p.sigma.toFixed(2) + " ch"}));
            fit.append($("<li>", {text: "Gaussian area: " + integral.toFixed(2) + " counts * ch"}));
            fit.append($("<li>", {text: "FWHM: " + FWHM.toFixed(2) + " ch"}));
            fit.append($("<li>", {text: "Resolution: " + (FWHM / p.mu * 100).toFixed(2) + "%"}));
            fit.append($("<li>", {text: "Total counts in range: " + p.sum_y.toFixed(0) + " counts"}));
            fit.append($("<li>", {text: "Counts in the peak: " + counts_in_peak.toFixed(2) + " counts"}));

            let label = $("<strong>", {text: "Fit n." + (p.data_index + 1)});

            let result = $("<li>");

            result.append(label).append(fit);

            fits_results.append(result);
        });

        return fits_results;
    }

    clear() {
        this.fitting_regions = [];
    }
}
