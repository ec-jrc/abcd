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

const CONFIG_JSON = "./config.json";

var zmq = require("zeromq");
//var zmq = {
//    "socket": function (type) {
//        return {
//            "type": type,
//            "connect": function (address) {
//                console.log("Connecting to: " + address);
//            },
//            "subscribe": function (topic) {
//                console.log("Subscribing to: " + topic);
//            },
//            "on": function (type, callback) {
//                console.log("On: " + type);
//            },
//            "send": function (message) {
//                console.log("Sending: " + message);
//            },
//        }
//    }
//}

var _ = require('lodash');
var express = require("express");
var http = require("http");
var socket_io = require("socket.io");
var moment = require("moment");

var favicon = require("serve-favicon");

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

function create_sub_socket(ip, bound_address, topic, callback) {
    // Creates a ZMQ SUB socket and returns it

    const address = get_address(bound_address, ip);
    console.log('Creating SUB socket with address: ' + address);
                            
    var sub_socket = zmq.socket('sub');
    sub_socket.connect(address);
    sub_socket.subscribe(topic);

    sub_socket.on('message', callback);

    return sub_socket;
}


function create_push_socket(ip, bound_address) {
    // Creates a ZMQ PUSH socket and returns it

    const address = get_address(bound_address, ip);
    console.log('Creating PUSH socket with address: ' + address);
                            

    var push_socket = zmq.socket('push');
    push_socket.connect(address);

    return push_socket;
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

var yargs = require('yargs');
 
var argv = yargs
    .usage('Usage: $0 -m <module> -d <module_dir> [options]\n\nWeb interface for ABCD modules')
    .option('m', {
        alias: 'module',
        describe: 'ABCD module name',
        type: 'string',
        nargs: 1,
        demand: true,
        demand: "A module name is required",
      })
    .option('d', {
        alias: 'module_dir',
        describe: 'Module directory with "public/" subdirectory',
        type: 'string',
        nargs: 1,
        demand: true,
        demand: "A module directory is required",
      })
    .option('f', {
        alias: 'config_file',
        describe: 'JSON Configuration file, with a "gui_config" field',
        type: 'string', /* array | boolean | string */
        nargs: 1,
        demand: false,
        default: CONFIG_JSON
      })
    .option('p', {
        alias: 'http_port',
        describe: 'HTTP port for the web interface',
        type: 'number', /* array | boolean | string */
        nargs: 1,
        demand: false,
        default: 8080
      })
    .option('v', {
        alias: 'verbose',
        describe: 'Enable verbose output',
        type: 'boolean',
        demand: false,
        default: false
      })
    .argv;

var module = argv.module;
var module_dir = argv.module_dir;
var config_json = argv.config_file
var http_port = argv.http_port;
var verbosity_level = argv.verbose ? 1 : 0;
var dirname = __dirname;

if (verbosity_level > 0) {
    console.log("This module is: '" + module + "'");
    console.log("The module directory is: '" + module_dir + "'");
    console.log("This directory is: '" + dirname + "'");
    console.log("Using configuration file: " + config_json);
    console.log("Using HTTP port: " + http_port);
    console.log("Verbose output: " + argv.verbose);
}

if (verbosity_level > 0) {
    process.on('exit', function (code) {
        console.log('Exiting with code: ' + code);
    });
}

process.on('uncaughtException', function (error) {
    if (error.code.includes("EADDRINUSE")) {
        console.error("ERROR: Port " + error.port + " is already in use, it should be changed");
    } else {
        console.error(error);
    }

    process.exit();
});

// In order to use require to read a JSON file
// we must provide a file path to it, so we add
// a './' in front.
if (!config_json.startsWith('/')) {
    config_json = './' + config_json;
}

var module_config = undefined;

try {
    module_config = require(config_json)["gui_config"];
} catch (error) {
    console.error("ERROR: Error on loading config file, exiting");
    process.exit()
}

var gui_events = [];
var zmq_events = [];
var zmq_sockets = module_config.sockets.filter(function (socket) {
        return !_.isEmpty(socket);
    }).map(function (socket) {

    if (socket.type == "status") {
        // If it is a status socket we just emit through socket.io the status message
        socket.socket = create_sub_socket(socket.ip, socket.address, socket.topic, function(binary_message) {
            const received = parse_pub_message(binary_message);

            // Broadcast status message
            io.emit("status_" + module, received.message);

            if (verbosity_level > 0) {
                console.log("Received status: " + JSON.stringify(received.message));
            }
        });

        return socket;
    }
    else if (socket.type == "events") {
        // If it is an events socket we save the event and then emit all the events
        socket.socket = create_sub_socket(socket.ip, socket.address, socket.topic, function(binary_message) {
            const received = parse_pub_message(binary_message);

            zmq_events.push(received.message);

            // Broadcast status message
            io.emit("event_" + module, zmq_events);

            console.log("Received event: " + JSON.stringify(received.message));
        });

        return socket;
    }
    else if (socket.type == "data") {
        // If it is a data socket we just emit the data message
        socket.socket = create_sub_socket(socket.ip, socket.address, socket.topic, function(binary_message) {
            const received = parse_pub_message(binary_message);

            // Broadcast status message
            io.emit("data_" + module, received.message);

            if (verbosity_level > 0) {
                console.log("Received data: " + JSON.stringify(received.message));
            }
        });

        return socket;
    }
    else if (socket.type == "commands") {
        // If it is a commands socket we just connect to the socket
        socket.socket = create_push_socket(socket.ip, socket.address);

        return socket;
    }
});

var io_sockets = {};

var msg_ID = 0;

// Creating here the io object in order to have it defined on the following functions
var io = {};

function socket_io_disconnection(id) {
    return function() {
        const timestamp = moment().toISOString();

        io_sockets[id]["connected"] = false;
        io_sockets[id]["disconnection_time"] = timestamp;

        const description = "A user disconnected, id: " + id;

        gui_events.push({"type": "event", "timestamp": moment(), "event": description});

        console.log(description);

        io.emit("event_gui", gui_events);
    }
}

function socket_io_connection(socket) {
    const timestamp = moment().toISOString();

    io_sockets[socket.id] = {"connection_time": timestamp, "connected": true, "disconnection_time": null};

    const description = "A user connected, id: " + socket.id;

    gui_events.push({"type": "event", "timestamp": moment(), "event": description});

    console.log(description);

    socket.on('disconnect', socket_io_disconnection(socket.id));

    zmq_sockets.filter(function (zmq_socket) {
        return zmq_socket.type == "commands";
    }).forEach(function (zmq_socket) {
        socket.on('command_' + module, socket_io_command(module, zmq_socket, socket.id));
    });

    socket.emit("event_" + module, zmq_events);

    // Broadcasting EFG events, because they are generated here and nothing is received
    io.emit("event_gui", gui_events);
}

function socket_io_command(module, zmq_socket, id) {
    return function(message) {
        console.log("Command to module: " + module + "; from socket id: " + id + "; message: " + JSON.stringify(message));

        zmq_socket.socket.send(JSON.stringify(message));
    }
}

var app = express();

app.use(express.static(module_dir + '/public'));

app.use(favicon(dirname + '/public/favicon.png'));
app.use('/shared', express.static(dirname + '/public'));

var server = http.createServer(app);
io = socket_io(server);

io.on("connection", socket_io_connection);

server.listen(http_port, function () {
    console.log('Server running at http://127.0.0.1:' + http_port);
});

setInterval(function () {
    const status_message = {"module": module, "timestamp": moment().toISOString(), "msg_ID": msg_ID, "sockets": io_sockets};

    io.emit("status_gui", status_message);

    msg_ID += 1;
}, module_config.publish_timeout * 1000);

