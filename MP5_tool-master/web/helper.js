var path = require('path'),
  fs = require('fs');

exports.resolve_pwd = function(dir, exep) {
  var layer = path.normalize(exep).split("/").length - 1;
  var tmp = [];
  for (var i = 0; i < layer; ++i) {
    tmp.push("..");
  }
  return path.resolve(dir, tmp.join("/"));
}

exports.read_cfg = function(cfg) {
  var args = fs.readFileSync(cfg).toString("utf8").trim().split("\n");
  var tmp = {};
  args.map(function(v) {
    v = v.split("=");
    tmp[v[0]] = v[1];
  });
  return tmp;
}
