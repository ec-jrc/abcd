"use strict";

var express = require('express');
var debug_init = require('debug')('wit:module:init');
var debug_messages = require('debug')('wit:module:messages');
var debug_get = require('debug')('wit:module:get');
var _ = require('lodash');
var zmq = require('zeromq');
var os = require("os");

const hostname = os.hostname();

var router = express.Router();

var app = null;
var logger = null;

var clean_configs = [];

var zmq_sockets_push = {};
var modules_events = {};

function parse_configs(modules_configs) {
  let clean_configs = [];

  for (let config of modules_configs) {
    if (_.has(config, "name") && _.has(config, "type") && _.has(config, "sockets")) {
      const module_name = config.name.replace(/\s+/g, '').replace(/[^\x00-\x7F]/g, "");
      const module_type = config.type;
      const module_description = String(_.get(config, "description", ""));

      if (_.has(clean_configs, module_name)) {
        console.error("ERROR: Module names should be unique. Repeated name: " + module_name);
      } else {
        debug_init("Found module: " + module_name + " of type: " + module_type + " and sockets: " + config.sockets.length);

        let sockets = [];

        for (const socket_config of config.sockets) {
          if (_.has(socket_config, "type") && _.has(socket_config, "address")) {
            const socket_type = String(socket_config.type);
            const socket_address = String(socket_config.address);
            // Topic might be empty!
            const socket_topic = String(_.get(socket_config, "topic", ""));

            debug_init("Found zmq socket of type: " + socket_type + " and address: " + socket_address + " (topic: " + socket_topic + ")");

            sockets.push({
              "type": socket_type,
              "address": socket_address,
              "topic": socket_topic
            });
          }
        }

        clean_configs.push({
          "name": module_name,
          "description": module_description,
          "type": module_type,
          "sockets": sockets
        });
      }
    }
  }

  return clean_configs;
}

async function create_zmq_socket(module_name, socket_config) {
  const socket_type = socket_config.type;
  const socket_address = socket_config.address;
  const socket_topic = socket_config.topic;

  // Faking a request and response for morgan
  let req = {
    'method': 'ZMQ',
    'url': "Creating zmq socket of type: " + socket_type + " and address: " + socket_address + " (topic: " + socket_topic + ")"
  };
  // With a true in headersSent morgan prints the status code
  let res = {
    'headersSent': true,
    'getHeader': (name) => '',
    'statusCode': 201,
    'statusMessage': 'Created'
  };

  logger(req, res, (err) => res);

  if (socket_type === "status" || socket_type === "data" || socket_type === "events") {
    var sock = new zmq.Subscriber();

    sock.connect(socket_address);
    sock.subscribe(socket_topic);

    // We need to use an array for potential multi-part messages, that are not
    // used in ABCD
    for await (const [message, ] of sock) {
      // The message is a Buffer
      const separator_index = _.findIndex(message, c => (c === 32));
      const topic = message.slice(0, separator_index).toString('ascii');
      const payload = message.slice(separator_index + 1);

      debug_messages(message);
      debug_messages(`Received a ${socket_type} message with topic: "${topic}" ${separator_index}`);
      debug_messages(typeof payload);

      try {
        let socket_io = app.get('socket_io');

        if (socket_type === "events") {
          const parsed_payload = JSON.parse(payload.toString('utf8'));

          modules_events[module_name].push(parsed_payload);

          socket_io.to(module_name).emit(socket_type, modules_events[module_name]);
        } else {
          socket_io.to(module_name).emit(socket_type, payload);
        }
      } catch (error) {
        console.error(error);
      }
    }
  } else if (socket_type === "commands") {
    var sock = new zmq.Push();

    sock.connect(socket_address);

    zmq_sockets_push[module_name].push(sock);
  }
}

function init_sockets(this_app, this_logger, modules_configs) {
  if (!_.isUndefined(modules_configs)) {
    clean_configs = parse_configs(modules_configs);
  }

  app = this_app;
  logger = this_logger;

  for (let config of clean_configs) {
    const module_name = config.name;
    const module_type = config.type;
    const module_sockets = config.sockets;

    debug_init("Creating sockets for module: " + module_name + " of type: " + module_type);

    zmq_sockets_push[module_name] = [];
    modules_events[module_name] = [];

    for (let socket_config of module_sockets) {
      create_zmq_socket(module_name, socket_config);
    }
  }
}

function create_router() {
  for (let config of clean_configs) {
    const module_name = config.name;
    const module_type = config.type;

    debug_init("Routing module: " + module_name + " of type: " + module_type);

    /* GET home page. */
    router.get('/' + String(module_name), function (req, res, next) {
      debug_get("Router call on: " + hostname + " with type: " + module_type + " and name: " + module_name);

      res.render(String(module_type),
        {
          'title': 'ABCD: ' + module_name,
          'hostname': hostname,
          'type': module_type,
          'name': module_name,
          'modules': clean_configs
        });
    });
  }

  return router;
}

function get_module_events(module_name) {
  return modules_events[module_name];
}

module.exports = {
  "set_config": modules_configs => { clean_configs = parse_configs(modules_configs); },
  "get_config": () => clean_configs,
  "init_sockets": init_sockets,
  "create_router": create_router,
  "get_sockets_push": () => zmq_sockets_push,
  "get_module_events": get_module_events
}
