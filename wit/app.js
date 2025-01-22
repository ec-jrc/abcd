"use strict";

/**
 * App dependencies.
 */

var express = require('express');
var path = require('path');
var _ = require('lodash');
var morgan = require('morgan');
const dayjs = require('dayjs');
var debug_init = require('debug')('wit:server:init');
var debug_connections = require('debug')('wit:server:connections');
var debug_messages = require('debug')('wit:server:messages');
var debug_heartbeat = require('debug')('wit:server:heartbeat');
var http = require('http');
const fs = require('fs');
const zmq = require("zeromq");
const { Server: IOServer } = require("socket.io");
const { ArgumentParser } = require('argparse');

var router_index = require('./routes/index');
var router_module = require('./routes/module');

/**
 * Default values.
 */

const HTTP_PORT = 8080;

/**
 * Parse command line arguments.
 */

const parser = new ArgumentParser({
  description: 'Web interface for ABCD',
  add_help: true
});

//parser.add_argument('-v', '--version', { action: 'version', version });
parser.add_argument('-p', '--http_port', {
  type: 'int',
  default: HTTP_PORT,
  help: 'HTTP port for the web interface, default: ' + HTTP_PORT
})
parser.add_argument('config_file', {
  type: 'str',
  help: 'Necessary configuration file'
});

const args = parser.parse_args();

/**
 * Definition of the logger
 */

var logger = morgan('dev');

/**
 * Load the settings
 */

debug_init("Configuration file: " + args.config_file);

const config_data = fs.readFileSync(args.config_file)
const config = JSON.parse(config_data);

const heartbeat = Number(_.get(config, "heartbeat", 500));

/**
 * Express initialization
 */

var app = express();

app.use('/static', express.static(path.join(__dirname, 'public/')));

// Defining the external modules paths that are needed for the web pages
app.use('/fira-sans', express.static(path.join(__dirname, 'node_modules/@fontsource/fira-sans/')));
app.use('/fira-mono', express.static(path.join(__dirname, 'node_modules/@fontsource/fira-mono/')));
app.use('/material-icons', express.static(path.join(__dirname, 'node_modules/@fontsource/material-icons/')));

app.use('/jquery', express.static(path.join(__dirname, 'node_modules/jquery/dist/')));
app.use('/dayjs', express.static(path.join(__dirname, 'node_modules/dayjs/')));
app.use('/plotly.js', express.static(path.join(__dirname, 'node_modules/plotly.js/dist/')));
app.use('/lodash', express.static(path.join(__dirname, 'node_modules/lodash/')));
app.use('/humanize-duration', express.static(path.join(__dirname, 'node_modules/humanize-duration/')));
app.use('/file-size', express.static(path.join(__dirname, 'node_modules/file-size/')));
app.use('/ace', express.static(path.join(__dirname, 'node_modules/ace-builds/src-noconflict/')));
app.use('/fmin', express.static(path.join(__dirname, 'node_modules/fmin/build/')));

// view engine setup
app.set('views', path.join(__dirname, 'views'));
app.set('view engine', 'pug');

app.use(logger);

app.use(express.json());
app.use(express.urlencoded({ extended: false }));
app.use(express.static(path.join(__dirname, 'public')));

router_module.init_sockets(app, logger, config.modules);

/**
 * Dictionary that returns the push sockets on which the modules are listening
 */

var modules_pushes_associations = router_module.get_sockets_push();

// The index router needs the configuration of the module router to render the
// list of available modules in the index page
app.use('/', router_index(router_module.get_config()));

app.use('/module', router_module.create_router());

// This is the last non-error-handling middleware, which means that:
//  1. no previous middleware responded
//  2. thus the user is requesting a page that does not exist
//  3. thus we should trigger an error (e.g. 404)
app.use(function(req, res, next) {
  // Reply only with a generic text page, in principle we should also have a
  // render for an HTML page and for a JSON object, because the page might have
  // been requested in that format.
  // Since we do not expect that users will request pages with a script, we will
  // only reply with a simple text page that a human can read.
  res.status(404);

  if (req.url.startsWith("/module")) {
    const module_name = req.url.slice("/module/".length);
    res.type("txt").send("ERROR: Module \"" + module_name + "\" not found, it should be enabled in the configuration")
  } else {
    res.type("txt").send("ERROR: Page not found");
  }

  // We are not calling next() because this should be the last middleware and
  // we want to stop the chain here.
});

// Error-handling middleware functions have the same form as non-error-handling
// middleware but require an arity of 4, where the first argument is the error.
// When there is an error in a connection, only error-handling middleware will
// be called.
app.use(function(err, req, res, next) {
  // It would be wise to hide the error message in a normal production
  // environment, but for this application we do not need to hide the
  // implementation details...

  res.status(err.status || 500);
  res.type("txt").send("ERROR " + err.status + ": " + err.message);
});

/**
 * Get port from the command line arguments and store in Express.
 */

//var port = normalizePort(process.env.PORT || '3000');
var port = normalizePort(args.http_port || HTTP_PORT);
app.set('port', port);

/**
 * Create HTTP server.
 */

var server = http.createServer(app);

/**
 * Enable socket.io.
 */

var socket_io = new IOServer(server);

/**
 * Dictionary that returns the modules to which the socket.io ids are connected
 */

var socket_io_modules_associations = {};

socket_io.on('connection', socket => {
  debug_connections("New socket id: " + socket.id);

  // Faking a request and response for morgan
  let req = {'method': 'SOCKET.IO',
             'url': 'connection id: ' + socket.id};
  // With a true in headersSent morgan prints the status code
  let res = {'headersSent': true,
             'getHeader': (name) => '',
             'statusCode': 201,
             'statusMessage': 'Created'};

  logger(req, res, (err) => res);

  socket.on('join_module', module_name => {
    debug_connections("Joining socket " + socket.id + " to room: " + module_name);
    socket.join(module_name);

    socket_io_modules_associations[socket.id] = module_name;

    socket.emit("acknowledge", "Joined to room: " + module_name);

    // Faking a request and response for morgan
    let req = {'method': 'SOCKET.IO',
               'url': 'joined socket id: ' + socket.id + " to room: " + module_name};
    // With a true in headersSent morgan prints the status code
    let res = {'headersSent': true,
               'getHeader': (name) => '',
               'statusCode': 201,
               'statusMessage': 'Created'};

    logger(req, res, (err) => res);

    // Updating the joined socket with the previous list of module events
    socket.emit("events", router_module.get_module_events(module_name));

    // Faking a request and response for morgan
    req = {'method': 'SOCKET.IO',
           'url': 'events list sent to socket id: ' + socket.id + " (room: " + module_name + ")"};
    // With a true in headersSent morgan prints the status code
    res = {'headersSent': true,
           'getHeader': (name) => '',
           'statusCode': 201,
           'statusMessage': 'Created'};

    logger(req, res, (err) => res);
  })

  socket.on('command', message => {
    try {
      const module_name = socket_io_modules_associations[socket.id];
      const zmq_sockets_push = modules_pushes_associations[module_name];

      debug_messages("Sending message from id: " + socket.id + " to module: " + module_name);

      for (let socket_push of zmq_sockets_push) {
        socket_push.send(message);
      }
    } catch (error) {
      console.error(error);
    }
  });

  socket.on('pong', message => {
    debug_heartbeat("Pong received from socket: " + socket.id);
  });

  socket.on('disconnect', error => {
    debug_connections("Socket id: " + socket.id + " ERROR: " + error);
  
    // Faking a request and response for morgan
    let req = {'method': 'SOCKET.IO',
               'url': 'disconnection of socket id: ' + socket.id + " (ERROR: " + error + ")"};
    // With a true in headersSent morgan prints the status code
    let res = {'headersSent': true,
               'getHeader': (name) => '',
               'statusCode': 408,
               'statusMessage': 'Disconnected'};
  
    logger(req, res, (err) => res);
  });
});

/**
 * Setting a heartbeat to the clients
 */
 
setInterval(function () {
  socket_io.emit('ping', { "timestamp" : dayjs() });
}, heartbeat);

/**
 * Registering the socket.io server to the app
 */
app.set('socket_io', socket_io);

/**
 * Listen on provided port, on all network interfaces.
 */
 
server.listen(port);
server.on('error', onError);
server.on('listening', onListening);
 
/**
 * Normalize a port into a number, string, or false.
 */

function normalizePort(val) {
  var port = parseInt(val, 10);

  if (isNaN(port)) {
    // named pipe
    return val;
  }

  if (port >= 0) {
    // port number
    return port;
  }

  return false;
}

/**
 * Event listener for HTTP server "error" event.
 */

function onError(error) {
  if (error.syscall !== 'listen') {
    throw error;
  }

  var bind = typeof port === 'string'
    ? 'Pipe ' + port
    : 'Port ' + port;

  // handle specific listen errors with friendly messages
  switch (error.code) {
    case 'EACCES':
      console.error(bind + ' requires elevated privileges');
      process.exit(1);
      break;
    case 'EADDRINUSE':
      console.error(bind + ' is already in use');
      process.exit(1);
      break;
    default:
      throw error;
  }
}

/**
 * Event listener for HTTP server "listening" event.
 */

function onListening() {
  var addr = server.address();
  var bind = typeof addr === 'string'
    ? 'pipe ' + addr
    : 'port ' + addr.port;
  debug_init('Listening on ' + bind);
}
 
