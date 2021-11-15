# `wit` - Web-Interface Two

`wit` is the new HTTP server for the web-based Graphical User Interface.
It allows to manage multiple modules of ABCD with a single HTTP server.

## Installation
`wit` is a server running on node.js, thus its dependencies may be installed with `npm`.
To launch the dependencies installation run inside the `wit` folder:
```
user@host$ npm install
```
`wit` requires the bindings of the ZeroMQ libraries and `npm` will try to compile and install them.
Sometimes at the end of the installation there might be warnings about securities vulnerabilities in some packages.
These are due to some of the dependencies and fixing those is above our powers.

## Configuration
There is an example configuration file that can help with the setup of `wit`: [`config.json`](./config.json) can send the updated configuration file to a running instance of `waan`.
Modules are identified by their `type`, that is the name of the executable of the module (_e.g._ `dasa`, `wadi`, `spec`, `tofcalc`, ...).
Multiple copies of the same module types may be controlled, and they can be distinguished by their unique `name` set in the configuration file.
`wit` interfaces the ZeroMQ sockets with the WebSockets for the GUI.
Each module then needs to have the list of the sockets that are to be interfaced.

## Running `wit`
To launch `wit` run inside its folder:
```
user@host$ node ./app.js ./config.json
```
The default HTTP port is 8080, so to connect to it just open a web-browser to the address: [http://localhost:8080](http://localhost:8080).
The browser will open a landing page, from which all the module interfaces are linked.

