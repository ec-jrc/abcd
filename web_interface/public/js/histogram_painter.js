
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

function HistogramPainter(DOM_ID, width, height) {
    const CIRCLE_RADIUS = width / 1000 * 5;

    var _this = this;

    this.DOM_ID = DOM_ID;
    this.plot_name = DOM_ID.replace(/[^\w]/gi, '')

    console.log("Creating an histogram with name: " + this.plot_name + "; width: " + width + "; height: " + height);

    // These are the default values for the plot
    this.margins = {top: 10, right: 50, bottom: 50, left: 50};

    this.width = width;
    this.height = height;

    this.plot_width = width - this.margins.left - this.margins.right;
    this.plot_height = height - this.margins.top - this.margins.bottom;

    // We need scales to convert data coordinates to image coordinates
    this.x_scale = d3.scaleLinear()
               .range([0, this.plot_width])
               .domain([-100, 100]);
    // SVG images are drawn from the top left corner, thus the range is backward
    this.y_scale = d3.scaleLinear()
               .range([this.plot_height, 0])
               .domain([0, 1]);

    this.x_axis = d3.axisBottom().scale(this.x_scale).ticks(10);
    this.y_axis = d3.axisLeft().scale(this.y_scale).ticks(10);

    this.svg = d3.select(DOM_ID);

    this.svg
        .attr("width", this.width)
        .attr("height", this.height)
        .style("background-color", 'white');

    // Adding a background rectangle so the downloaded image is displayed correctly
    this.svg.append("rect")
        .attr("width", this.width)
        .attr("height", this.height)
        .attr("fill", "white");

    this.spectrum_line = d3.line()
                           .curve(d3.curveStepAfter)
                           .x(function(d) {return _this.x_scale(d.x);})
                           .y(function(d) {return _this.y_scale(d.y);})

    const dummy_data = [{"x": -100, "y": 0}, {"x": +100, "y": 0}];

    this.svg.append("path")
            .datum(dummy_data)
            .attr("class", "histo_line")
            .attr("d", this.spectrum_line)
            .attr("transform", "translate(" + _this.margins.left + "," + _this.margins.top + ")")
            .attr('stroke', '#1f77b4')
            .attr('stroke-width', 2)
            .attr('fill', 'none');

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

    // Adding a circle that highlights the selected bin
    this.bin_highlighter = this.svg
        .append("circle")
        .data([{"x": 0, "y": 0}])
        .attr("cx", function(d, i) {return _this.x_scale(d.x);})
        .attr("cy", function(d, i) {return _this.y_scale(d.y);})
        .attr("transform", "translate(" + _this.margins.left + "," + _this.margins.top + ")")
        .attr("r", CIRCLE_RADIUS)
        .style("stroke", "#ff7f0e")
        .style("stroke-opacity", 0)
        .attr('stroke-width', 2)
        .style("fill", 'none')
        .style("fill-opacity", 0);

    // Clears the labels and circle when the mouse is outside the plot
    this.svg.on('mouseout', function () {
        _this.x_label
            .data([0])
            .text(function(d) {return '';});
        _this.y_label
            .data([0])
            .text(function(d) {return '';});
        _this.bin_highlighter
            .style("stroke-opacity", 0);
    });
}

HistogramPainter.prototype.update_display = function (histo, select_y_scale) {
    if (histo.data.length > 0) {
        var _this = this;

        this.histo = histo;

        // We use a y offset because in D3 the line should be continuous
        // so there cannot be the holes of the invalid points.
        var y_offset = 0;

        if(select_y_scale === "logarithmic") {
            console.log("Using log scale");

            y_offset = 1;

            this.y_scale = d3.scaleLog()
                             .base(10)
                             .range([this.plot_height, 0])
                             .domain([d3.min(histo.data) + y_offset, d3.max(histo.data) + y_offset]);
        } else {
            console.log("Using linear scale");

            this.y_scale = d3.scaleLinear()
                             .range([this.plot_height, 0])
                             .domain([0, d3.max(histo.data)]);
        }

        this.y_axis.scale(this.y_scale);

        this.x_scale.domain([histo.config.min, histo.config.max]);
        this.x_axis.scale(this.x_scale);

        this.spectrum_line = d3.line()
                               .curve(d3.curveStepAfter)
                               .x(function(d) { return _this.x_scale(d.x); })
                               .y(function(d) { return _this.y_scale(d.y + y_offset); })

        const delta = (histo.config.max - histo.config.min) / histo.config.bins;

        const data = histo.data.map(function (value, index) {
            return {"x": index * delta + histo.config.min, "y": value};
        });

        var histo_line = this.svg.selectAll(".histo_line")
                                 .datum(data)
                                 .attr("d", this.spectrum_line);

        this.svg
            .select(".x_axis")
            //.transition(t)
            .call(this.x_axis);
        this.svg
            .select(".y_axis")
            //.transition(t)
            .call(this.y_axis);

        // Enabling the display of the labels and the highlighting circle
        this.svg.on('mousemove', function () {
            try {
                const coordinates = d3.mouse(this);
                const x = _this.x_scale.invert(coordinates[0] - _this.margins.left);
                //const y = _this.y_scale.invert(coordinates[1]);
        
                const index = Math.floor((x - histo.config.min) / delta);

                const edge = index * delta + histo.config.min;
                const counts = histo.data[index];
        
                _this.x_label
                    .data([edge])
                    .text(function(d) {return 'Bin edge: ' + d.toFixed(2);});
                _this.y_label
                    .data([counts])
                    .text(function(d) {return 'Counts: ' + d.toFixed(0);});

                _this.bin_highlighter
                    .data([{"x": edge, "y": counts}])
                    .attr("cx", function(d, i) {return _this.x_scale(d.x);})
                    .attr("cy", function(d, i) {return _this.y_scale(d.y + y_offset);})
                    .style("stroke-opacity", 1);
            } catch (error) {}
        });
    }
}

HistogramPainter.prototype.download_image = function () {
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

HistogramPainter.prototype.download_data = function () {
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
