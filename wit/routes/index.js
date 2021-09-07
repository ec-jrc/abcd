var express = require('express');
var _ = require("lodash/core");
var os = require("os");

const hostname = os.hostname();

var router = express.Router();

module.exports = function(modules_config) {
  /* GET home page. */
  router.get('/', function(req, res, next) {
    res.render('index', {'title': 'ABCD landing page',
                         'hostname': hostname,
                         'modules': modules_config});
  });

  return router;
};