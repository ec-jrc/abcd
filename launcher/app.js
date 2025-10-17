"use strict";

/**
 * App dependencies.
 */

var express = require('express');
var path = require('path');
var morgan = require('morgan');
var debug = require('debug')('launcher:server');
var http = require('http');
const { ArgumentParser } = require('argparse');

/**
 * Default values.
 */

const HTTP_PORT = 8888;

/**
 * Parse command line arguments.
 */

const parser = new ArgumentParser({
  description: 'ABCD launcher',
  add_help: true
});

//parser.add_argument('-v', '--version', { action: 'version', version });
parser.add_argument('-p', '--http_port', {
  type: 'int',
  default: HTTP_PORT,
  help: `HTTP port for the web interface, default: ${HTTP_PORT}`
})

const args = parser.parse_args();

/**
 * Definition of the logger
 */

var logger = morgan('dev');

/**
 * Express initialization
 */

var app = express();

app.use('/static', express.static(path.join(__dirname, 'public/')));

// Defining the external modules paths that are needed for the web pages
app.use('/fira-sans', express.static(path.join(__dirname, 'node_modules/@fontsource/fira-sans/')));
app.use('/fira-mono', express.static(path.join(__dirname, 'node_modules/@fontsource/fira-mono/')));
app.use('/svgdotjs', express.static(path.join(__dirname, 'node_modules/@svgdotjs/svg.js/dist/')));
app.use('/svgdraggable', express.static(path.join(__dirname, 'node_modules/@svgdotjs/svg.draggable.js/dist/')));

// view engine setup
app.set('views', path.join(__dirname, 'views'));
app.set('view engine', 'pug');

app.use(logger);

app.use(express.json());
app.use(express.urlencoded({ extended: false }));
app.use(express.static(path.join(__dirname, 'public')));

// The index router needs the configuration of the module router to render the
// list of available modules in the index page
app.use('/', (req, res, next) => {
  res.render('index', { 'title': 'ABCD launcher' });
});

// This is the last non-error-handling middleware, which means that:
//  1. no previous middleware responded
//  2. thus the user is requesting a page that does not exist
//  3. thus we should trigger an error (e.g. 404)
app.use(function (req, res, next) {
  // Reply only with a generic text page, in principle we should also have a
  // render for an HTML page and for a JSON object, because the page might have
  // been requested in that format.
  // Since we do not expect that users will request pages with a script, we will
  // only reply with a simple text page that a human can read.
  res.status(404).type("txt").send("ERROR: Page not found");

  // We are not calling next() because this should be the last middleware and
  // we want to stop the chain here.
});

// Error-handling middleware functions have the same form as non-error-handling
// middleware but require an arity of 4, where the first argument is the error.
// When there is an error in a connection, only error-handling middleware will
// be called.
app.use(function (err, req, res, next) {
  // It would be wise to hide the error message in a normal production
  // environment, but for this application we do not need to hide the
  // implementation details...

  res.status(err.status || 500).type("txt").send("ERROR " + err.status + ": " + err.message);
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
  debug('Listening on ' + bind);
}
