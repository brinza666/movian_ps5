

exports.writeFileSync = function(filename, data, opts) {
  var fs = require('native/fs');
  var fd = fs.open(filename, 'w');
  try {
    fs.write(fd, data, 0, null, 0);
  }
  finally {
    Core.resourceDestroy(fd);
  }
}

exports.readFileSync = function(filename, opts) {
  var fs = require('native/fs');
  var fd = fs.open(filename, 'r');
  try {
    var buf = new Duktape.Buffer(fs.fsize(fd));
	//var buf = new Uint8Array.plainOf(fs.fsize(fd));
	//fs.read(fd, buf.buffer, 0, f_size, 0);
    fs.read(fd, buf.valueOf(), 0, buf.length, 0);
    return buf;
  } finally {
    Core.resourceDestroy(fd);
  }
}

exports.readdirSync = function(path) {
  return require('native/fs').readdir(path);
}

exports.unlinkSync = function(filename) {
  require('native/fs').unlink(filename);
}

exports.mkdirSync = function(path) {
  require('native/fs').mkdirs(path);
}

exports.rmdirSync = function(path) {
  require('native/fs').rmdir(path);
}