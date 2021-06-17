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

const DEFAULTS_JSON = "./defaults.json";
var defaults_json = DEFAULTS_JSON;

function print_usage(argv) {
    console.log("Usage: " + process.argv0 + " " + process.argv[1] + " [options]");
    console.log("\t-h: Display this message");
    console.log("\t-f <file_name>: EFG configuration file, default: " + DEFAULTS_JSON);

    process.exit(0);
}

process.argv.forEach(function (val, index, array) {
    if (val.startsWith("-h")) {
        print_usage();
    }
    else if (val.startsWith("-f")) {
        if (val.length > 2) {
            defaults_json = val.slice(2);
        } 
        else if (array[index + 1].length > 0) {
            defaults_json = array[index + 1];
        }
        else
        {
            print_usage();
        }
    }
});

if (!defaults_json.startsWith('/')) {
    defaults_json = './' + defaults_json;
}

console.log("Using configuration file: " + defaults_json);

const abcd_defaults = require(defaults_json);

var zmq = require("zeromq");

var express = require("express");
var http = require("http");
var socket_io = require("socket.io");
var moment = require("moment");

var favicon = require("serve-favicon");

var events = {"abcd": [], "efg": [], "hijk": [], "lmno": [], "pqrs": []};
var sockets = {};
var msg_ID = 0;

function get_address(bind_address, ip) {
    // Joins ip and bind_address, the latter expressed in the standard ZMQ way.
    //
    // E.g.
    //
    //  > get_address("tcp://*:16180", "127.0.0.1")
    //  'tcp://127.0.0.1:16180'
    //
    //  > get_address("tcp://192.168.0.2:16180", "127.0.0.1")
    //  'tcp://192.168.0.2:16180'
    //
    const sep = '*';

    const sep_position = bind_address.indexOf(sep);

    if (sep_position === -1) {
        return bind_address;
    } else {
        return bind_address.slice(0, sep_position) + ip + bind_address.slice(sep_position + 1);
    }
}

function parse_pub_message(binary_message) {
    // Parses a message from a PUB socket, isolating the topic and the payload.
    // Returns an object with members:
    //   "topic": a string
    //   "message": an object

    const sep = ' ';

    const envelope = binary_message.toString('ascii');
    const sep_position = envelope.indexOf(sep);
    const topic = envelope.slice(0, sep_position);
    const json_message = envelope.slice(sep_position + 1);
    const message = JSON.parse(json_message);

    return {"topic": topic, "message": message};
}

function launch_sub_socket(defaults, module, data_type, topic, callback) {
    // Creates a ZMQ SUB socket and returns it

    const bind_address = defaults[module][data_type + "_address"];
    const ip = defaults[module]["ip"];

    const address = get_address(bind_address, ip);
    console.log(module + ' ' + data_type + ' address: ' + address);
                            
    var sub_socket = zmq.socket('sub');
    sub_socket.connect(address);
    sub_socket.subscribe(topic);

    sub_socket.on('message', callback);

    return sub_socket;
}


function generate_commands_socket(defaults, module) {
    var bind_address = defaults[module]["commands_address"];
    var ip = defaults[module]["ip"];

    var commands_address = get_address(bind_address, ip);
    console.log(module + ' commands address: ' + commands_address);
                            

    var commands_socket = zmq.socket('push');
    commands_socket.connect(commands_address);

    return commands_socket;
}

var commands = {};
commands["abcd"] = generate_commands_socket(abcd_defaults, "abcd");
commands["hijk"] = generate_commands_socket(abcd_defaults, "hijk");
commands["lmno"] = generate_commands_socket(abcd_defaults, "lmno");
commands["pqrs"] = generate_commands_socket(abcd_defaults, "pqrs");

// Creating here the io object in order to have it defined on the following functions
var io = {};

function socket_io_disconnection(id) {
    return function() {
        const timestamp = moment().toISOString();

        sockets[id]["connected"] = false;
        sockets[id]["disconnection_time"] = timestamp;

        const description = "A user disconnected, id: " + id;

        events["efg"].push({"type": "event", "timestamp": moment(), "event": description});

        console.log(description);

        io.emit("event_efg", events["efg"]);
    }
}

function socket_io_connection(socket) {
    const timestamp = moment().toISOString();

    sockets[socket.id] = {"connection_time": timestamp, "connected": true, "disconnection_time": null};

    const description = "A user connected, id: " + socket.id;

    events["efg"].push({"type": "event", "timestamp": moment(), "event": description});

    console.log(description);

    socket.on('disconnect', socket_io_disconnection(socket.id));
    socket.on('command_abcd', socket_io_command("abcd", socket.id));
    socket.on('command_hijk', socket_io_command("hijk", socket.id));
    socket.on('command_lmno', socket_io_command("lmno", socket.id));
    socket.on('command_pqrs', socket_io_command("pqrs", socket.id));

    socket.emit("event_abcd", events["abcd"]);
    socket.emit("event_hijk", events["hijk"]);
    socket.emit("event_lmno", events["lmno"]);
    socket.emit("event_pqrs", events["pqrs"]);

    // Broadcasting EFG events, because they are generated here and nothing is received
    io.emit("event_efg", events["efg"]);
}

function socket_io_command(module, id) {
    return function(message) {
        console.log("Command to module: " + module + "; from socket id: " + id + "; message: " + JSON.stringify(message));

        commands[module].send(JSON.stringify(message));
    }
}

var app = express();

app.use(favicon(__dirname + '/public/favicon.png'));
app.use(express.static(__dirname + '/public'));

var server = http.createServer(app);
io = socket_io(server);

io.on("connection", socket_io_connection);

server.listen(abcd_defaults["efg"]["http_port"], function () {
    console.log('Server running at http://127.0.0.1:' + abcd_defaults["efg"]["http_port"]);
});

//process.on('SIGTERM', function () {
//    console.log('Exiting for a SIGTERM');
//    server.close(function () {
//        process.exit(0);
//    });
//});

//process.on('SIGINT', function (code) {
//    console.log('Exiting for a SIGINT');
//    server.close(function () {
//        process.exit(0);
//    });
//});

//process.on('uncaughtException', function (error) {
//    console.log('ERROR: ' + error);
//    server.close(function () {
//        process.exit(1);
//    });
//});

process.on('exit', function (code) {
    console.log('Exiting with code: ' + code);
});

var abcd_status = launch_sub_socket(abcd_defaults, "abcd", "status", "status", function(binary_message) {
    const received = parse_pub_message(binary_message);

    // Broadcast abcd status message
    io.emit("status_abcd", received["message"]);
});

var abcd_events = launch_sub_socket(abcd_defaults, "abcd", "status", "events", function(binary_message) {
    const received = parse_pub_message(binary_message);
    events["abcd"].push(received["message"]);

    // Broadcast abcd event message
    io.emit("event_abcd", events["abcd"]);

    console.log("Received ABCD event: " + Object.keys(received["message"]));
});

var hijk_status = launch_sub_socket(abcd_defaults, "hijk", "status", "status", function(binary_message) {
    const received = parse_pub_message(binary_message);

    // Broadcast hijk status message
    io.emit("status_hijk", received["message"]);
});

var hijk_events = launch_sub_socket(abcd_defaults, "hijk", "status", "events", function(binary_message) {
    const received = parse_pub_message(binary_message);
    events["hijk"].push(received["message"]);

    // Broadcast hijk event message
    io.emit("event_hijk", events["hijk"]);

    console.log("Received HIJK event: " + Object.keys(received["message"]));
});

var lmno_status = launch_sub_socket(abcd_defaults, "lmno", "status", "status", function(binary_message) {
    const received = parse_pub_message(binary_message);

    // Broadcast lmno status message
    io.emit("status_lmno", received["message"]);
});

var lmno_events = launch_sub_socket(abcd_defaults, "lmno", "status", "events", function(binary_message) {
    const received = parse_pub_message(binary_message);
    events["lmno"].push(received["message"]);

    // Broadcast lmno event message
    io.emit("event_lmno", events["lmno"]);

    console.log("Received LMNO event: " + Object.keys(received["message"]));
});

var pqrs_status = launch_sub_socket(abcd_defaults, "pqrs", "status", "status", function(binary_message) {
    const received = parse_pub_message(binary_message);

    // Broadcast pqrs status message
    io.emit("status_pqrs", received["message"]);
});

var pqrs_events = launch_sub_socket(abcd_defaults, "pqrs", "status", "events", function(binary_message) {
    const received = parse_pub_message(binary_message);
    events["pqrs"].push(received["message"]);

    // Broadcast pqrs event message
    io.emit("event_pqrs", events["pqrs"]);

    console.log("Received PQRS event: " + Object.keys(received["message"]));
});

var pqrs_data = launch_sub_socket(abcd_defaults, "pqrs", "data", "data", function(binary_message) {
    const received = parse_pub_message(binary_message);

    // Broadcast pqrs data message
    io.emit("data_pqrs", received["message"]);
});

var wafi_data = launch_sub_socket(abcd_defaults, "wafi", "data", "data_wafi_waveforms", function(binary_message) {
    const received = parse_pub_message(binary_message);

    // Broadcast pqrs data message
    io.emit("data_wafi", received["message"]);
});

setInterval(function () {
    const status_message = {"module": "efg", "timestamp": moment().toISOString(), "msg_ID": msg_ID, "sockets": sockets};

    io.emit("status_efg", status_message);

    msg_ID += 1;
}, abcd_defaults["efg"]["publish_timeout"] * 1000);
