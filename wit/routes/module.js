"use strict";

var express = require('express');
var debug = require('debug')('wit:module');
var debug_messages = require('debug')('wit:module:messages');
var _ = require('lodash');
var zmq = require('zeromq');
var os = require("os");

const hostname = os.hostname();

var router = express.Router();

var clean_configs = [];

var zmq_sockets_sub = {};
var zmq_sockets_push = {};
var modules_events = {};

function parse_configs (modules_configs) {
  let clean_configs = [];

  for (let config of modules_configs) {
    if (_.has(config, "name") && _.has(config, "type") && _.has(config, "sockets")) {
      const module_name = config.name.replace(/\s+/g, '').replace(/[^\x00-\x7F]/g, "");
      const module_type = config.type;
      const module_description = String(_.get(config, "description", ""));

      if (_.has(clean_configs, module_name)) {
        console.error("ERROR: Module names should be unique. Repeated name: " + module_name);
      } else {
        debug("Found module: " + module_name + " of type: " + module_type + " and sockets: " + config.sockets.length);

        let sockets = [];

        for (const socket_config of config.sockets) {
          if (_.has(socket_config, "type") && _.has(socket_config, "address")) {
            const socket_type = String(socket_config.type);
            const socket_address = String(socket_config.address);
            // Topic might be empty!
            const socket_topic = String(_.get(socket_config, "topic", ""));

            debug("Found zmq socket of type: " + socket_type + " and address: " + socket_address + " (topic: " + socket_topic + ")");
            
            sockets.push({"type": socket_type,
                          "address": socket_address,
                          "topic": socket_topic
                         });
          }
        }

        clean_configs.push({"name": module_name,
                            "description": module_description,
                            "type": module_type,
                            "sockets": sockets
                           });
      }
    }
  }

  return clean_configs;
}

function create_zmq_socket(socket_io, module_name, socket_config) {
  const socket_type = socket_config.type;
  const socket_address = socket_config.address;
  const socket_topic = socket_config.topic;

  
  debug("Creating zmq socket of type: " + socket_type + " and address: " + socket_address + " (topic: " + socket_topic + ")");

  if (socket_type === "status" || socket_type === "data") {
    var sock = zmq.socket('sub');
    sock.connect(socket_address);
    sock.subscribe(socket_topic);

    sock.on('message', function(message) {
      // The message is a Buffer
      const separator_index = _.findIndex(message, c => (c === 32));
      const topic = message.slice(0, separator_index).toString('ascii');
      const payload = message.slice(separator_index + 1);

      debug_messages('Received a message with topic: ' + topic);

      // At this moment the socket_io should be available...
      socket_io.to(module_name).emit(socket_type, payload);
    });

    zmq_sockets_sub[module_name].push(sock);

  } else if (socket_type === "events") {
    var sock = zmq.socket('sub');
    sock.connect(socket_address);
    sock.subscribe(socket_config.topic);

    sock.on('message', function(message) {
      // The message is a Buffer
      const separator_index = _.findIndex(message, c => (c === 32));
      const topic = message.slice(0, separator_index).toString('ascii');
      const payload = message.slice(separator_index + 1).toString('utf8');

      debug_messages('Received a message with topic: ' + topic);

      modules_events[module_name].push(payload);

      socket_io.to(module_name).emit(socket_type, modules_events);
    });

    zmq_sockets_sub[module_name].push(sock);

  } else if (socket_type === "commands") {
    var sock = zmq.socket('push');
    sock.connect(socket_address);

    zmq_sockets_push[module_name].push(sock);
  }
}

function create_router(modules_configs) {
  if (!_.isUndefined(modules_configs)) {
    clean_configs = parse_configs(modules_configs);
  }

  for (let config of clean_configs) {
    const module_name = config.name;
    const module_type = config.type;
    const module_sockets = config.sockets;

    debug("Routing module: " + module_name + " of type: " + module_type + " and sockets: " + config.sockets.lenght);

    zmq_sockets_sub[module_name] = [];
    zmq_sockets_push[module_name] = [];
    modules_events[module_name] = [];

    /* GET home page. */
    router.get('/' + String(module_name), function(req, res, next) {
      debug("Router call on: " + hostname + " with type: " + module_type + " and name: " + module_name);

      const socket_io = req.app.get('socketio');

      for (let socket_config of module_sockets) {
        create_zmq_socket(socket_io, module_name, socket_config);
      }

      res.render(String(module_type),
                 {'title': 'ABCD: ' + module_name,
                  'hostname': hostname,
                  'type': module_type,
                  'name': module_name});
    });

  }

  return router;
}

module.exports = {
  "set_config": modules_configs => { clean_configs = parse_configs(modules_configs); },
  "get_config": () => clean_configs,
  "create_router": modules_configs => {
    if (!_.isUndefined(modules_configs)) {
      clean_configs = parse_configs(modules_configs);
    }

    return create_router(clean_configs);
  }
}