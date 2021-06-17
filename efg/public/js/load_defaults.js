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

function load_defaults(defaults)
{
    var socket_types = ['commands_address', 'data_address', 'status_address'];
    var modules = ['abcd', 'efg', 'hijk', 'lmno', 'pqrs'];

    for (var j = 0; j < socket_types.length; j++)
    {
        var socket_type = socket_types[j];

        console.log(socket_type)

        for (i = 0; i < modules.length; i++)
        {
            var module = modules[i]
            var address = defaults[module][socket_type];

            if (address)
            {
                if (address.indexOf("tcp") !== -1)
                {
                    var tokens = address.split(":");

                    $("<li>").text(module + ": TCP port: " + tokens[2]).appendTo($("#" + socket_type));
                }
                else
                {
                    $("<li>").text(module + ": socket: " + address).appendTo($("#" + socket_type));
                }
            }
        }
    }

    for (var i = 0; i < modules.length; i++)
    {
        var module = modules[i]
        var IP = defaults[module]["ip"];

        if (IP)
        {
            $("<li>").text(module + ": IP: " + IP).appendTo($("#IPs"));
        }
    }

    $("<li>").text("abcd status: " + defaults["abcd"]["status_topic"]).appendTo($("#pub_topics"));
    $("<li>").text("abcd events: " + defaults["abcd"]["events_topic"]).appendTo($("#pub_topics"));
    $("<li>").text("abcd data events: " + defaults["abcd"]["data_events_topic"]).appendTo($("#pub_topics"));
    $("<li>").text("abcd data waveforms: " + defaults["abcd"]["data_waveforms_topic"]).appendTo($("#pub_topics"));
    
    $("<li>").text("hijk status: " + defaults["hijk"]["status_topic"]).appendTo($("#pub_topics"));
    $("<li>").text("hijk events: " + defaults["hijk"]["events_topic"]).appendTo($("#pub_topics"));

    $("<li>").text("lmno status: " + defaults["lmno"]["status_topic"]).appendTo($("#pub_topics"));
    $("<li>").text("lmno events: " + defaults["lmno"]["events_topic"]).appendTo($("#pub_topics"));
    $("<li>").text("pqrs data: " + defaults["pqrs"]["data_topic"]).appendTo($("#pub_topics"));

    $("#efg_http_port").text("EFG: " + defaults["efg"]["http_port"]);

    for (var i = 0; i < modules.length; i++)
    {
        var module = modules[i]
        var base_period = defaults[module]["base_period"];

        if (base_period)
        {
            $("<li>").text(module + ": " + base_period + " ms").appendTo($("#base_periods"));
        }
    }

    var caen_modules = ["abcd", "hijk"];

    for (var i = 0; i < caen_modules.length; i++)
    {
        var module = caen_modules[i];
        var ul = $("<ul>");

        $("<li>").text("Connection type: " + defaults[module]["connection_type"]).appendTo(ul);
        $("<li>").text("Link number: " + defaults[module]["connection_type"]).appendTo(ul);
        $("<li>").text("CONET node: " + defaults[module]["CONET_node"]).appendTo(ul);
        $("<li>").text("VME address: " + defaults[module]["connection_type"]).appendTo(ul);

        $("#caen_configs").append($("<li>").text(module + ":").append(ul));
    }
}

function page_loaded() {
    console.log("Loading defaults")
    $.getJSON("js/defaults.json", load_defaults);
}

$(document).ready(page_loaded)
