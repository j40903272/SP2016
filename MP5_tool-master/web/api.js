var fs = require('fs');

exports.threads_active = function(req, res) {
  res.contentType('json');
  if (!req.session.proc) {
    res.send({
      success: null,
      error: "authorization fail"
    });
    return;
  }
  try {
    process.kill(req.session.proc.pid, "SIGUSR1");
  } catch (error) {
    console.log(error);
    res.send({
      success: null,
      error: "kill process error"
    });
    return;
  }
  try {
    var fifo_path = req.session.proc.args.run_path + "/csiebox_server." + req.session.proc.pid;
    // console.log(fifo_path);
  } catch (error) {
    console.log(error);
    res.send({
      success: null,
      error: "no found run_path in config"
    });
    return;
  }

  var tid = setTimeout(function() {
    console.log("timeout!!!!");
    res.send({
      success: null,
      error: "open timeout"
    });
  }, 3000);

  fs.open(fifo_path, "r", function(err, fd) {
    clearTimeout(tid);
    if (err) {
      console.log(err);
      res.send({
        success: null,
        error: "open error"
      });
      return;
    }
    try {
      var buf = new Buffer(4);
      fs.readSync(fd, buf, 0, 4);
      fs.closeSync(fd);
      res.send({
        success: buf.readInt32BE(0),
        error: null
      });
      return;
    } catch (error) {
      console.log(error);
      res.send({
        success: null,
        error: "read fifo error"
      });
      return;
    }
  });
}
