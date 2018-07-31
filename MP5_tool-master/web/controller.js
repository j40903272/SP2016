var exec = require('child_process').exec,
  config = require('./config'),
  helper = require('./helper'),
  fs = require('fs'),
  path = require('path');

exports.index = function(req, res) {
  if (!req.session.proc) {
    res.redirect("/choose");
    return;
  }
  res.render("index", {
    proc: req.session.proc
  });
}

exports.choose = function(req, res) {
  exec('ps x -o pid,command | grep csiebox_server | grep -v grep',
    function(err, stdout, stderr) {
      if (err) {
        res.render("choose", {
          error: "ps error",
          pids: null
        });
      }
      var pids = []
      stdout.split("\n").forEach(function(v) {
        var arr = v.trim().split(" ");
        if (arr.length == 1) {
          return;
        }
        pids.push({
          pid: arr[0],
          cmd: arr.slice(1).join(" "),
        });
      });
      var error = null;
      if (pids.length == 0) {
        error = "cannot find csiebox_server";
      }
      res.render("choose", {
        error: error,
        pids: pids
      });
    });
}

exports.choose_post = function(req, res) {
  if (!("pid" in req.body)) {
    res.redirect("/choose");
    return;
  }
  exec('ps x -o pid,command | grep csiebox_server | grep ' + req.body.pid + ' | grep -v grep',
    function(err, stdout, stderr) {
      if (err) {
        res.redirect("/choose");
        return;
      }
      var arr = stdout.trim().split(" ");
      var pwd = helper.resolve_pwd(config.BINDIR, arr[1]);
      var cfg = path.resolve(pwd, arr[2]);
      var args = helper.read_cfg(cfg);
      console.log(args);
      req.session.proc = {
        pid: parseInt(arr[0], 10),
        cmd: arr.slice(1).join(" "),
        cfg: cfg,
        args: args
      };
      console.log(req.session.proc);
      res.redirect("/");
      return;
    });
}
