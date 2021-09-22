"use strict";

/**
 * App dependencies.
 */

var createError = require('http-errors');
var express = require('express');
var path = require('path');
//var cookieParser = require('cookie-parser');
var lessMiddleware = require('less-middleware');
var _ = require('lodash');
var morgan = require('morgan');
const dayjs = require('dayjs');
var debug = require('debug')('wit:server');
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
 * Definition of the reference to Socket.io
 */

var io = {};

/**
 * Load the settings
 */

debug("Configuration file: " + args.config_file);

const config_data = fs.readFileSync(args.config_file)
const config = JSON.parse(config_data);

const heartbeat = Number(_.get(config, "heartbeat", 500));

var io_modules_associations = {};
var modules_pushes_associations = {};

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
//app.use(cookieParser());
app.use(lessMiddleware(path.join(__dirname, 'public')));
app.use(express.static(path.join(__dirname, 'public')));

router_module.init_sockets(app, logger, config.modules);
modules_pushes_associations = router_module.get_sockets_push();

app.use('/module', router_module.create_router());


app.use('/', router_index(router_module.get_config()));

// catch 404 and forward to error handler
app.use(function(req, res, next) {
  next(createError(404));
});

// error handler
app.use(function(err, req, res, next) {
  // set locals, only providing error in development
  res.locals.message = err.message;
  res.locals.error = req.app.get('env') === 'development' ? err : {};

  // render the error page
  res.status(err.status || 500);
  res.render('error');
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
io = new IOServer(server);

io.on('connection', socket => {
  debug("New socket id: " + socket.id);

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
    debug("Joining socket " + socket.id + " to room: " + module_name);
    socket.join(module_name);

    io_modules_associations[socket.id] = module_name;

    socket.emit("acknowledge", "Joined to room: " + module_name);

    // Faking a request and response for morgan
    let req = {'method': 'SOCKET.IO',
               'url': 'joining socket id: ' + socket.id + " to room: " + module_name};
    // With a true in headersSent morgan prints the status code
    let res = {'headersSent': true,
               'getHeader': (name) => '',
               'statusCode': 201,
               'statusMessage': 'Created'};

    logger(req, res, (err) => res);
  })

  socket.on('command', message => {
    try {
      const module_name = io_modules_associations[socket.id];
      const zmq_sockets_push = modules_pushes_associations[module_name];

      debug("Sending message from id: " + socket.id + " to module: " + module_name);

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
    debug("Socket id: " + socket.id + " ERROR: " + error);
  
    // Faking a request and response for morgan
    let req = {'method': 'SOCKET.IO',
               'url': 'disconnection id: ' + socket.id + " ERROR: " + error};
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
  io.emit('ping', { "timestamp" : dayjs() });
}, heartbeat);

/**
 * Registering the socket.io server to the app
 */
app.set('socketio', io);

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
 