exports.register = function(callback) {
  var net = require('net');
  try {
    var client = net.connect({port: 10090}, function() {
      client.write("2web" + process.env.USER);
    });
    var result = "";
    client.on('data', function(data) {
      result = data.readInt32BE(0);
      client.end();
    });
    client.on('end', function() {
      if (result > 0) {
        callback(result);
      } else {
        console.log("register fail");
      }
    });
  } catch (err) {
    console.log("cannot find port_register");
  }
};
