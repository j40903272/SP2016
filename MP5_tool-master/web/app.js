var express = require('express'),
  config = require('./config'),
  register = require('./register'),
  controller = require('./controller'),
  api = require('./api'),
  http = require('http'),
  path = require('path');

register.register(function(port) {
  var app = express(),
    server = http.createServer(app);

  app.configure(function() {
    app.set('port', port);
    app.set('views', path.join(__dirname, 'views'));
    app.set('view engine', 'ejs');
    app.use(express.favicon());
    app.use(express.logger('dev'));
    app.use(express.bodyParser());
    app.use(express.methodOverride());
    app.use(express.cookieParser());
    app.use(express.session({ secret: '1234567890QWERTY' }));
    app.use(app.router);
    app.use(express.static(path.join(__dirname, 'public')));
  });

  app.configure('development', function (){
    app.use(express.errorHandler({ dumpExceptions: true, showStack: true }));
  });

  app.configure( 'production', function (){
    app.use(express.errorHandler());
  });

  app.get('/', controller.index);
  app.get('/choose', controller.choose);
  app.post('/choose', controller.choose_post);
  app.post('/api/threads_active', api.threads_active);

  server.listen(app.get('port'), function() {
    console.log("Express server listening on port: " + app.get('port'));
  });
});
