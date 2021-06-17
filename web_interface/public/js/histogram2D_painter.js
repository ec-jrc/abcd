
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

function Histogram2DPainter(DOM_ID, width, height) {
    const CIRCLE_RADIUS = width / 1000 * 5;
    this.colors = ["#ffffff", "#ffffd9", "#edf8b1", "#c7e9b4", "#7fcdbb", "#41b6c4", "#1d91c0", "#225ea8", "#253494", "#081d58"];

    var _this = this;

    this.DOM_ID = DOM_ID;
    this.plot_name = DOM_ID.replace(/[^\w]/gi, '')

    console.log("Creating an histogram2D with name: " + this.plot_name + "; width: " + width + "; height: " + height);

    // These are the default values for the plot
    this.margins = {top: 10, right: 50, bottom: 50, left: 50};

    this.width = width;
    this.height = height;

    this.plot_width = width - this.margins.left - this.margins.right;
    this.plot_height = height - this.margins.top - this.margins.bottom;

    // We need scales to convert data coordinates to image coordinates
    this.x_scale = d3.scaleLinear()
               .range([0, this.plot_width])
               .domain([0, 1000]);
    // SVG images are drawn from the top left corner, thus the range is backward
    this.y_scale = d3.scaleLinear()
               .range([this.plot_height, 0])
               .domain([-0.7, 1]);

    this.x_axis = d3.axisBottom().scale(this.x_scale).ticks(10);
    this.y_axis = d3.axisLeft().scale(this.y_scale).ticks(10);

    // We need scales to convert canvas coordinates to PSD coordinates
    this.inverse_x_scale = d3.scaleLinear()
               .domain([0, this.plot_width - 1])
               .range([0, this.plot_width - 1]);
    // SVG images are drawn from the top left corner, thus the range is backward
    this.inverse_y_scale = d3.scaleLinear()
               .domain([this.plot_height - 1, 0])
               .range([0, this.plot_height - 1]);

    this.red_colors = this.colors.map(function (value, index) {
        return parseInt('0x' + value.substr(1, 2));
    });
    this.green_colors = this.colors.map(function (value, index) {
        return parseInt('0x' + value.substr(3, 2));
    });
    this.blue_colors = this.colors.map(function (value, index) {
        return parseInt('0x' + value.substr(5, 2));
    });

    this.container = d3.select(DOM_ID)
        .style("width", this.width + 'px')
        .style("height", this.height + 'px');
    
    this.svg = this.container.append('svg:svg')
        .attr("width", this.width)
        .attr("height", this.height)
        .style("margin", "0px")
        .style("position", "absolute")
        .style("background-color", 'white');

    // Add embedded canvas to embedded body
    // The '-1' in the left margin is to avoid to
    // draw over the Y axis line.
    this.canvas = this.container.append("canvas")
        .attr("width", this.plot_width)
        .attr("height", this.plot_height)
        .style("position", "absolute")
        .style("margin-left", (this.margins.left + 1) + 'px')
        .style("margin-top", this.margins.top + 'px')
        .style("margin-right", this.margins.right + 'px')
        .style("margin-bottom", this.margins.bottom + 'px');
    
    // Get drawing context of canvas
    this.canvas_context = this.canvas.node().getContext("2d");

    // Adding a background rectangle to the SVG so the
    // downloaded image is displayed correctly
    this.svg.append("rect")
        .attr("width", this.width)
        .attr("height", this.height)
        .attr("fill", "white");

    // Add the X Axis
    this.svg
        .append("g")
        .attr("class", "x_axis")
        .attr("transform", "translate(" + this.margins.left + ", " + (this.plot_height + this.margins.top) + ")")
        .call(this.x_axis);

    // Add the Y Axis
    this.svg
        .append("g")
        .attr("class", "y_axis")
        .attr("transform", "translate(" + this.margins.left + ", " + this.margins.top + ")")
        .call(this.y_axis);

    // Add the labels with the bin position and counts
    this.x_label = this.svg
        .append('text')
        .attr('text-anchor', 'end')
        .data([0, 0])
        .attr('x', this.width - this.margins.right)
        .attr('y', this.margins.top * 2)
        .attr('fill', '#000')
        .text(function(d) {return '';});
    this.y_label = this.svg
        .append('text')
        .attr('text-anchor', 'end')
        .data([0, 0])
        .attr('x', this.width - this.margins.right)
        .attr('y', this.margins.top  * 4)
        .attr('fill', '#000')
        .text(function(d) {return '';});
}

Histogram2DPainter.prototype.update_display = function (histo, select_color_scale) {
    if (histo.data.length > 0) {
        var _this = this;

        console.log("Updating display for 2D histo with bins_x: " + histo.config.bins_x + " bins_y: " + histo.config.bins_y);
        console.log("min_x: " + histo.config.min_x + " max_x: " + histo.config.max_x);
        console.log("min_y: " + histo.config.min_y + " max_y: " + histo.config.max_y);

        this.histo = histo;

        this.y_scale.domain([histo.config.min_y, histo.config.max_y]);
        this.y_axis.scale(this.y_scale);

        this.x_scale.domain([histo.config.min_x, histo.config.max_x]);
        this.x_axis.scale(this.x_scale);

        this.inverse_y_scale.range([0, histo.config.bins_y - 1]);
        this.inverse_x_scale.range([0, histo.config.bins_x - 1]);

        const max_value = d3.max(histo.data);
        var value_offset = 0;

        var color_scale = null;

        if(select_color_scale === "logarithmic") {
            // We use a value offset because if we want to use
            // a logarithmic color scale we don't want zeroes.
            const min_value = d3.min(histo.data);
            value_offset = 1;

            console.log("Using logarithmic color scale between " + min_value + " and " + max_value);

            color_scale = d3.scaleLog().base(10).range([0, this.colors.length - 1])
                .domain([min_value + value_offset, max_value + value_offset]);
        } else {
            console.log("Using linear color scale between 0 and " + max_value);

            color_scale = d3.scaleLinear().range([0, this.colors.length - 1])
                .domain([0, max_value]);
        }

        var new_image_data = this.canvas_context.createImageData(this.plot_width, this.plot_height);

        console.log("Created new image data with length: " + new_image_data.data.length);

        for (var canvas_x = 0; canvas_x < this.plot_width; canvas_x++) {
            for (var canvas_y = 0; canvas_y < this.plot_height; canvas_y++) {

                var histo_x = Math.floor(this.inverse_x_scale(canvas_x));
                var histo_y = Math.floor(this.inverse_y_scale(canvas_y));

                var histo_index = (histo_x + histo.config.bins_x * histo_y);

                var histo_value = histo.data[histo_index] + value_offset;

                // The canvas data is composed of groups of 4 bytes 
                // with the RGBA order
                var canvas_index = (canvas_x + this.plot_width * canvas_y) * 4;

                var color_index = Math.floor(color_scale(histo_value));

                new_image_data.data[canvas_index + 0] = this.red_colors[color_index];
                new_image_data.data[canvas_index + 1] = this.green_colors[color_index];
                new_image_data.data[canvas_index + 2] = this.blue_colors[color_index];
                new_image_data.data[canvas_index + 3] = 255;

                //console.log("x: " + histo_x + " y: " + histo_y + " i: " + histo_index + " v: " + histo_value + " color_i: " + color_index);
            }
        }

        this.canvas_context.putImageData(new_image_data, 0, 0);
                
        this.svg
            .select(".x_axis")
            //.transition(t)
            .call(this.x_axis);
        this.svg
            .select(".y_axis")
            //.transition(t)
            .call(this.y_axis);
    }
}

Histogram2DPainter.prototype.download_image = function () {
    var plot = d3.select(this.DOM_ID).node();

    console.log(plot);

    var XMLS = new XMLSerializer();
    var xml_text = XMLS.serializeToString(plot);

    // Add name spaces for the XML
    if(!xml_text.match(/^<svg[^>]+xmlns="http\:\/\/www\.w3\.org\/2000\/svg"/)){
        xml_text = xml_text.replace(/^<svg/, '<svg xmlns="http://www.w3.org/2000/svg"');
    }
    if(!xml_text.match(/^<svg[^>]+"http\:\/\/www\.w3\.org\/1999\/xlink"/)){
        xml_text = xml_text.replace(/^<svg/, '<svg xmlns:xlink="http://www.w3.org/1999/xlink"');
    }

    // Add xml declaration
    xml_text = '<?xml version="1.0" standalone="no"?>\r\n' + xml_text;

    create_and_download_file(xml_text, this.plot_name + ".svg", "svg");
}

Histogram2DPainter.prototype.download_data = function () {
    try {
        var csv_text = 'edge,counts\r\n';

        const delta = (this.histo.config.max - this.histo.config.min) / this.histo.config.bins;

        for (var index = 0; index < this.histo.data.length; index += 1) {
            const edge = index * delta + this.histo.config.min;
            const counts = this.histo.data[index];

            csv_text += "" + edge + "," + counts + "\r\n";
        }

        create_and_download_file(csv_text, this.plot_name + ".csv", "txt");
    } catch (error) {
        console.error(error);
    }
}
