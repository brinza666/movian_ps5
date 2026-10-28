/* Movian ECMAScript API v2 on Duktape. Runs inside the payload. */
var mov_services = [];
var mov_routes = [];
var mov_proxy_pairs = [];
var mov_seq = 1;
var mov_fds = {};
var mov_fd_seq = 3;
var mov_kv = {};
var mov_timers = [];
var mov_global = null;
var mov_cache = {};

function mov_log_line() {
  var s = "";
  var i;
  for (i = 0; i < arguments.length; i++) {
    if (i) s += " ";
    s += arguments[i];
  }
  mov_log(s);
}

var console = { log: mov_log_line };
var print = mov_log_line;

function setTimeout(fn) {
  var id = ++mov_seq;
  mov_timers.push({ id: id, fn: fn });
  return id;
}
function clearTimeout(id) {
  var i;
  for (i = 0; i < mov_timers.length; i++) {
    if (mov_timers[i].id === id) {
      mov_timers.splice(i, 1);
      return;
    }
  }
}
function __movianFlush() {
  var guard = 0;
  while (mov_timers.length && guard < 8) {
    var batch = mov_timers;
    mov_timers = [];
    var i;
    for (i = 0; i < batch.length; i++) {
      try { batch[i].fn(); } catch (e) { mov_log(String(e)); }
    }
    guard++;
  }
  return guard;
}

if (typeof Duktape === "undefined") Duktape = {};
if (!Duktape.fin) Duktape.fin = function () {};
Duktape.Buffer = function (n) {
  this.length = n || 0;
  this._s = "";
  this.toString = function () { return this._s; };
  this.valueOf = function () { return this; };
};

var RealProxy = Proxy;
Proxy = new RealProxy(RealProxy, {
  construct: function (target, args) {
    var created = Reflect.construct(target, args);
    mov_proxy_pairs.push(created, args[0]);
    return created;
  }
});

function mov_raw(v) {
  if (!v || typeof v !== "object") return null;
  var i;
  for (i = 0; i < mov_proxy_pairs.length; i += 2) {
    if (mov_proxy_pairs[i] === v) return mov_proxy_pairs[i + 1];
  }
  if (v["\xffnode"] === 1) return v;
  return null;
}

function mov_make() {
  var node = {};
  node["\xffnode"] = 1;
  node["\xffname"] = "";
  node["\xffvalue"] = undefined;
  node["\xffkids"] = Object.create(null);
  node["\xfforder"] = [];
  node["\xffsubs"] = [];
  node["\xffparent"] = null;
  node["\xffzombie"] = false;
  return node;
}

function mov_fire(node, op, a, b) {
  var subs = node["\xffsubs"];
  var copy = subs.slice(0);
  var i;
  for (i = 0; i < copy.length; i++) {
    try { copy[i].fn(op, a, b); } catch (e) { mov_log("sub " + e); }
  }
}

function mov_child(node, name) {
  var key = String(name);
  var kids = node["\xffkids"];
  var next = kids[key];
  if (!next) {
    next = mov_make();
    next["\xffname"] = key;
    next["\xffparent"] = node;
    kids[key] = next;
    if (key.charCodeAt(0) !== 255)
      node["\xfforder"].push(next);
  }
  return next;
}

var np = {
  global: function () { return mov_global; },
  create: function (name) {
    var n = mov_make();
    n["\xffname"] = name ? String(name) : "";
    return n;
  },
  getChild: function (node, name) {
    var n = mov_raw(node);
    if (!n) return mov_make();
    return mov_child(n, name);
  },
  getValue: function (node) {
    var n = mov_raw(node);
    return n ? n["\xffvalue"] : undefined;
  },
  isValue: function (value) { return !!mov_raw(value); },
  set: function (node, name, value) {
    var n = mov_raw(node);
    if (!n) return;
    var c = mov_child(n, name);
    c["\xffvalue"] = value;
    mov_fire(c, "set", value);
  },
  setRichStr: function (node, name, value) { np.set(node, name, value); },
  has: function (node, name) {
    var n = mov_raw(node);
    if (!n) return false;
    return Object.prototype.hasOwnProperty.call(n["\xffkids"], String(name));
  },
  enumerate: function (node) {
    var n = mov_raw(node);
    var out = [];
    if (!n) return out;
    var k;
    for (k in n["\xffkids"]) out.push(k);
    return out;
  },
  deleteChild: function (node, name) {
    var n = mov_raw(node);
    if (!n) return true;
    var key = String(name);
    var c = n["\xffkids"][key];
    if (!c) return true;
    delete n["\xffkids"][key];
    var order = n["\xfforder"];
    var i;
    for (i = order.length - 1; i >= 0; i--) if (order[i] === c) order.splice(i, 1);
    return true;
  },
  subscribe: function (node, fn) {
    var n = mov_raw(node);
    if (!n || typeof fn !== "function") return 0;
    var sub = { node: n, fn: fn };
    n["\xffsubs"].push(sub);
    return sub;
  },
  setParent: function (childNode, parentNode) {
    var c = mov_raw(childNode);
    var p = mov_raw(parentNode);
    if (!c || !p) return;
    if (c["\xffparent"]) {
      var old = c["\xffparent"]["\xfforder"];
      var i;
      for (i = old.length - 1; i >= 0; i--) if (old[i] === c) old.splice(i, 1);
    }
    c["\xffparent"] = p;
    p["\xfforder"].push(c);
  },
  destroy: function (node) {
    var n = mov_raw(node);
    if (!n) return;
    n["\xffzombie"] = true;
    mov_fire(n, "destroyed");
  },
  isZombie: function (node) {
    var n = mov_raw(node);
    return !!(n && n["\xffzombie"]);
  },
  isSame: function (a, b) {
    var x = mov_raw(a);
    var y = mov_raw(b);
    return !!(x && x === y);
  },
  haveMore: function () {},
  print: function () {},
  deleteChilds: function (node) {
    var n = mov_raw(node);
    if (n) n["\xfforder"] = [];
  },
  sendEvent: function (node, type, value) {
    var n = mov_raw(node);
    if (n) mov_fire(n, type, value);
  },
  atomicAdd: function (node, delta) {
    var n = mov_raw(node);
    if (!n) return 0;
    var v = Number(n["\xffvalue"] || 0) + Number(delta || 0);
    n["\xffvalue"] = v;
    return v;
  },
  makeUrl: function () { return "prop:" + (++mov_seq); },
  unloadDestroy: function () {},
  moveBefore: function () {},
  setClipRange: function () {},
  select: function () {},
  link: function (src, dst) {
    var a = mov_raw(src);
    var b = mov_raw(dst);
    if (a && b) b["\xffvalue"] = a["\xffvalue"];
  },
  getName: function (node) {
    var n = mov_raw(node);
    return n ? n["\xffname"] : "";
  }
};

var mov_nav_url = "";
var mov_nav_opened = false;
var mov_page_root = null;
var mov_setting_nodes = [];
var mov_item_opt_nodes = [];
var mov_send_event = np.sendEvent;
np.sendEvent = function (node, type, value) {
  if ((type === "redirect" || type === "redirect2") && value != null && value !== "") {
    mov_nav_url = String(value);
    mov_nav_opened = false;
  }
  return mov_send_event(node, type, value);
};

function mov_dirname(path) {
  var s = String(path);
  var i = s.lastIndexOf("/");
  return i <= 0 ? "" : s.substring(0, i);
}
function mov_basename(path) {
  var s = String(path);
  var i = s.lastIndexOf("/");
  return i < 0 ? s : s.substring(i + 1);
}
function mov_join(dir, rel) {
  var parts = String(dir || "").split("/");
  var more = String(rel || "").split("/");
  var i;
  for (i = 0; i < more.length; i++) {
    if (more[i] === "" || more[i] === ".") continue;
    if (more[i] === "..") parts.pop();
    else parts.push(more[i]);
  }
  var out = [];
  for (i = 0; i < parts.length; i++) if (parts[i] !== "") out.push(parts[i]);
  return out.join("/");
}

var mov_alias = {
  "showtime/page": "movian/page",
  "showtime/http": "movian/http",
  "showtime/service": "movian/service",
  "showtime/settings": "movian/settings",
  "showtime/html": "movian/html",
  "showtime/itemhook": "movian/itemhook",
  "showtime/prop": "movian/prop",
  "showtime/store": "movian/store",
  "showtime/xml": "movian/xml",
  fs: "modules/fs.js",
  url: "modules/url.js",
  querystring: "modules/querystring.js",
  http: "modules/http.js",
  https: "modules/https.js"
};

function mov_resolve(request, from) {
  var name = mov_alias[request] || request;
  if (name.indexOf("movian/") === 0) return "modules/movian/" + name.substring(7) + ".js";
  if (name.indexOf("modules/") === 0) return name;
  if (request.charAt(0) === ".") {
    var base = mov_join(from, request);
    if (mov_source(base) != null) return base;
    if (mov_source(base + ".js") != null) return base + ".js";
  }
  return null;
}

function mov_wrap_settings(mod) {
  var real = mod.globalSettings;
  if (typeof real !== "function" || mod.__wrapped) return;
  var current = null;
  mod.globalSettings = function () {
    var args = Array.prototype.slice.call(arguments);
    var F = function () { return real.apply(this, args); };
    F.prototype = real.prototype;
    current = new F();
    return current;
  };
  var keys = ["createBool", "createString", "createInt", "createDivider", "createInfo", "createAction", "createMultiOpt"];
  var i;
  for (i = 0; i < keys.length; i++) {
    (function (key) {
      mod[key] = function () {
        if (!current || typeof current[key] !== "function")
          throw new Error("settings." + key + " before globalSettings");
        return current[key].apply(current, arguments);
      };
    })(keys[i]);
  }
  mod.__wrapped = 1;
}

function mov_require(request, from) {
  if (request === "native/prop") return np;
  if (request === "native/service") return mov_native_service;
  if (request === "native/route") return mov_native_route;
  if (request === "native/hook") return mov_native_hook;
  if (request === "native/io") return mov_native_io;
  if (request === "native/string") return mov_native_string;
  if (request === "native/fs") return mov_native_fs;
  if (request === "native/kvstore") return mov_native_kv;
  if (request === "native/metadata") return mov_native_meta;
  if (request === "native/misc") return mov_native_misc;
  if (request === "native/popup") return mov_native_popup;
  if (request === "native/crypto") return mov_native_crypto;
  if (request === "native/subtitle") return mov_native_sub;
  if (request === "native/websocket") return mov_native_ws;
  if (request === "native/gumbo") return mov_native_gumbo;
  if (request === "native/htsmsg") return mov_native_hts;
  var path = mov_resolve(request, from || "");
  if (!path) throw new Error("Cannot find module " + request);
  if (mov_cache[path]) return mov_cache[path];
  var src = mov_source(path);
  if (src == null) throw new Error("Cannot find module " + request);
  var exports = {};
  var module = { exports: exports };
  mov_cache[path] = exports;
  var fn = new Function("exports", "module", "require", "__filename", "__dirname", src);
  fn(exports, module, function (name) { return mov_require(name, mov_dirname(path)); }, path, mov_dirname(path));
  mov_cache[path] = module.exports;
  if (path === "modules/movian/settings.js") mov_wrap_settings(module.exports);
  return module.exports;
}

var require = function (name) { return mov_require(name, ""); };

function mov_query(url, args) {
  if (!args || typeof args !== "object") return url;
  var parts = [];
  var k;
  for (k in args) {
    if (!Object.prototype.hasOwnProperty.call(args, k)) continue;
    parts.push(encodeURIComponent(k) + "=" + encodeURIComponent(String(args[k])));
  }
  if (!parts.length) return url;
  return url + (url.indexOf("?") >= 0 ? "&" : "?") + parts.join("&");
}

function mov_do_http(url, ctrl) {
  var c = ctrl && typeof ctrl === "object" ? ctrl : {};
  var target = String(url);
  if (target.indexOf("http://") !== 0 && target.indexOf("https://") !== 0) {
    if (c.noFail) return { buffer: "", responseheaders: [], statuscode: 0 };
    throw new Error("Only http and https");
  }
  if (c.args && typeof c.args === "object" && !(c.args instanceof Array))
    target = mov_query(target, c.args);
  var headers = [];
  var h = c.headers;
  var k;
  if (h && typeof h === "object") {
    for (k in h) if (Object.prototype.hasOwnProperty.call(h, k)) headers.push(k + ": " + h[k]);
  }
  var method = String(c.method || "GET");
  var post = c.postdata == null ? "" : String(c.postdata);
  var raw = mov_http(target, method, headers.join("\n"), post, c.noFail ? 1 : 0);
  if (!raw || raw.error) {
    if (c.noFail) return { buffer: "", responseheaders: [], statuscode: 0 };
    throw new Error(raw && raw.error ? raw.error : "http failed");
  }
  var body = raw.body != null ? raw.body : "";
  var buffer = {
    length: body.length,
    toString: function () { return body; },
    valueOf: function () { return body; }
  };
  return {
    buffer: buffer,
    responseheaders: raw && raw.headers ? raw.headers : [],
    statuscode: raw && raw.statuscode ? raw.statuscode : 0
  };
}

var mov_native_io = {
  httpReq: function (url, ctrl, cb) {
    try {
      var res = mov_do_http(url, ctrl);
      if (typeof cb === "function") { cb(null, res); return; }
      return res;
    } catch (err) {
      if (typeof cb === "function") { cb(err, null); return; }
      if (ctrl && ctrl.noFail) return { buffer: "", responseheaders: [], statuscode: 0 };
      throw err;
    }
  },
  probe: function () { return {}; },
  httpInspectorCreate: function () { return 0; },
  xmlrpc: function () { throw new Error("xmlrpc is not linked"); }
};

var mov_native_string = {
  isUtf8: function (bytes) {
    var s = typeof bytes === "string" ? bytes : (bytes && bytes.toString ? bytes.toString() : "");
    return mov_is_utf8(s) ? true : false;
  },
  utf8FromBytes: function (bytes, encoding) {
    var s = typeof bytes === "string" ? bytes : (bytes && bytes.toString ? bytes.toString() : "");
    var enc = String(encoding || "").toLowerCase();
    if (enc.indexOf("1251") >= 0 || enc.indexOf("windows") >= 0) return mov_cp1251(s);
    if (!enc && !mov_is_utf8(s)) return mov_cp1251(s);
    return s;
  },
  entityDecode: function (s) {
    var out = String(s);
    out = out.replace(/&nbsp;/gi, " ");
    out = out.replace(/&amp;/g, "&");
    out = out.replace(/&lt;/g, "<");
    out = out.replace(/&gt;/g, ">");
    out = out.replace(/&quot;/g, "\"");
    out = out.replace(/&apos;|&#39;/g, "'");
    out = out.replace(/&#(\d+);/g, function (_, n) {
      var c = parseInt(n, 10);
      return c > 0 && c < 1114112 ? String.fromCharCode(c) : _;
    });
    return out;
  },
  queryStringSplit: function (s) {
    var out = {};
    var q = String(s).replace(/^\?/, "");
    if (!q) return out;
    var parts = q.split("&");
    var i;
    for (i = 0; i < parts.length; i++) {
      var p = parts[i].split("=");
      var key = decodeURIComponent(p[0] || "");
      var val = decodeURIComponent(p.slice(1).join("=") || "");
      out[key] = val;
    }
    return out;
  },
  pathEscape: function (s) { return encodeURIComponent(String(s)); },
  paramEscape: function (s) { return encodeURIComponent(String(s)); },
  durationToString: function (n) {
    var s = Math.max(0, Math.floor(Number(n) || 0));
    var m = Math.floor(s / 60);
    var r = s % 60;
    return m + ":" + (r < 10 ? "0" : "") + r;
  },
  parseURL: function (value) {
    var s = String(value);
    var m = /^([A-Za-z][A-Za-z0-9+.-]*:)(\/\/)?([^\/?#]*)([^?#]*)(\?[^#]*)?(#.*)?$/.exec(s);
    if (!m) return { href: s, protocol: "", host: "", hostname: "", port: "", pathname: s, search: "", hash: "" };
    var host = m[3] || "";
    var hostname = host;
    var port = "";
    var c = host.lastIndexOf(":");
    if (c > 0 && host.indexOf("]") < 0) {
      hostname = host.substring(0, c);
      port = host.substring(c + 1);
    }
    return {
      protocol: m[1] || "",
      slashes: true,
      host: host,
      hostname: hostname,
      port: port,
      pathname: m[4] || "",
      search: m[5] || "",
      hash: m[6] || "",
      href: s
    };
  },
  resolveURL: function (base, rel) {
    var r = String(rel);
    var b = String(base);
    if (/^[A-Za-z][A-Za-z0-9+.-]*:/.test(r)) return r;
    var p = mov_native_string.parseURL(b);
    if (r.indexOf("//") === 0) return p.protocol + r;
    var origin = p.protocol + "//" + p.host;
    if (r.charAt(0) === "/") return origin + r;
    if (r.charAt(0) === "?") return origin + p.pathname + r;
    if (r.charAt(0) === "#") return origin + p.pathname + (p.search || "") + r;
    var path = p.pathname || "/";
    var slash = path.lastIndexOf("/");
    var dir = slash >= 0 ? path.substring(0, slash + 1) : "/";
    return origin + dir + r;
  }
};

var mov_native_fs = {
  mkdirs: function (path) { mov_mkdirs(String(path)); },
  dirname: function (path) { return mov_dirname(path); },
  basename: function (path) { return mov_basename(path); },
  copyfile: function () {},
  open: function (path, mode) {
    var id = mov_fd_seq++;
    var m = String(mode || "r");
    var buf = "";
    if (m.indexOf("r") >= 0) {
      var got = mov_read(String(path));
      if (got != null) buf = String(got);
    }
    mov_fds[id] = { path: String(path), mode: m, buf: buf };
    return id;
  },
  write: function (fd, data) {
    var slot = mov_fds[fd];
    if (!slot) return;
    slot.buf += data == null ? "" : String(data);
  },
  fsize: function (fd) { return mov_fds[fd] ? mov_fds[fd].buf.length : 0; },
  read: function (fd, target) {
    var slot = mov_fds[fd];
    if (!slot || !target) return;
    if (typeof target === "object") target._s = slot.buf;
  }
};

var mov_native_kv = {
  getInteger: function (url, domain, key, def) {
    var v = mov_kv[String(key)];
    return v == null ? def : Number(v);
  },
  getBoolean: function (url, domain, key, def) {
    var v = mov_kv[String(key)];
    if (v == null) return def;
    return v === "true" || v === "1";
  },
  getString: function (url, domain, key) { return mov_kv[String(key)] || ""; },
  set: function (url, domain, key, value) { mov_kv[String(key)] = String(value); }
};

var mov_native_meta = { videoMetadataBind: function () { return 0; }, bindPlayInfo: function () {} };
var mov_native_misc = {
  systemIpAddress: function () { return "0.0.0.0"; },
  selectView: function () {},
  cachePut: function () {},
  cacheGet: function () { return null; }
};
var mov_native_popup = {
  message: function () { return true; },
  notify: function (msg) { mov_log(String(msg)); },
  textDialog: function () { return ""; },
  getAuthCredentials: function () { return null; }
};
var mov_native_crypto = {
  hashCreate: function (algo) { return { algo: String(algo || "md5"), data: "" }; },
  hashUpdate: function (hash, data) { if (hash) hash.data += String(data == null ? "" : data); },
  hashFinalize: function (hash) {
    if (!hash) return "";
    if (hash.algo.indexOf("sha") >= 0) return mov_sha1(hash.data);
    return mov_md5(hash.data);
  }
};
var mov_native_sub = { getLanguages: function () { return []; }, addProvider: function () { return 0; }, addItem: function () {} };
var mov_native_ws = { clientCreate: function () { return 0; }, clientSend: function () {} };
var mov_native_hts = { get: function () {}, print: function () {}, xmlDeser: function () { return { msg: {} }; } };

var mov_native_service = {
  create: function (origin, title, url, type) {
    var item = { title: String(title == null ? "" : title), url: String(url == null ? "" : url), type: String(type == null ? "" : type) };
    if (item.url.indexOf(":") >= 0) mov_services.push(item);
    return ++mov_seq;
  },
  enable: function () { return true; }
};
var mov_native_route = {
  create: function (re, cb) {
    var compiled;
    try { compiled = new RegExp("^(?:" + re + ")$"); }
    catch (e) { compiled = null; mov_log("route " + e); }
    mov_routes.push({ re: compiled, source: String(re), cb: cb });
    return ++mov_seq;
  },
  backendOpen: function (page, url) {
    var target = String(url);
    var raw = mov_raw(page);
    if (!raw || raw === mov_page_root) {
      mov_nav_url = target;
      mov_nav_opened = true;
      mov_open_into(target, mov_page_root);
      return;
    }
    mov_open_into(target, raw);
  }
};
var mov_native_hook = {
  register: function () { return ++mov_seq; }
};

function mov_el(name) {
  return { kind: "el", name: name, attrs: [], children: [], text: "" };
}
function mov_text(text) {
  return { kind: "text", name: "#text", attrs: [], children: [], text: text };
}
var mov_voids = { area: 1, base: 1, br: 1, col: 1, embed: 1, hr: 1, img: 1, input: 1, link: 1, meta: 1, param: 1, source: 1, track: 1, wbr: 1 };
function mov_parse_html(html) {
  var root = { kind: "doc", name: "#document", attrs: [], children: [], text: "" };
  var stack = [root];
  var s = String(html == null ? "" : html);
  var i = 0;
  while (i < s.length) {
    if (s.charAt(i) !== "<") {
      var j = s.indexOf("<", i);
      var end = j < 0 ? s.length : j;
      var text = s.substring(i, end);
      if (text) stack[stack.length - 1].children.push(mov_text(text));
      i = end;
      continue;
    }
    if (s.substring(i, i + 4) === "<!--") {
      var c = s.indexOf("-->", i + 4);
      i = c < 0 ? s.length : c + 3;
      continue;
    }
    if (s.substring(i, i + 2) === "<!" || s.substring(i, i + 2) === "<?") {
      var d = s.indexOf(">", i);
      i = d < 0 ? s.length : d + 1;
      continue;
    }
    var gt = s.indexOf(">", i);
    if (gt < 0) break;
    var close = s.charAt(i + 1) === "/";
    var inner = s.substring(i + (close ? 2 : 1), gt).replace(/^\s+|\s+$/g, "");
    i = gt + 1;
    if (close) {
      var cname = inner.split(/\s/)[0].toLowerCase();
      var k;
      for (k = stack.length - 1; k > 0; k--) {
        if (stack[k].name === cname) { stack.length = k; break; }
      }
      continue;
    }
    var cut = /^([^\s\/]+)([\s\S]*)$/.exec(inner);
    if (!cut) continue;
    var name = cut[1].toLowerCase();
    var attrs = [];
    var attrSrc = cut[2].replace(/\/\s*$/, "");
    var re = /([^\s=\/>]+)(?:\s*=\s*(?:"([^"]*)"|'([^']*)'|([^\s"'=<>`]+)))?/g;
    var am;
    while ((am = re.exec(attrSrc))) attrs.push({ name: am[1], value: am[2] || am[3] || am[4] || "" });
    var node = mov_el(name);
    node.attrs = attrs;
    stack[stack.length - 1].children.push(node);
    var self = /\/\s*$/.test(inner) || mov_voids[name];
    if (!self) {
      stack.push(node);
      if (name === "script" || name === "style") {
        var endTag = new RegExp("</" + name + "\\s*>", "i");
        var rest = s.substring(i);
        var found = endTag.exec(rest);
        var rawText = found ? rest.substring(0, found.index) : rest;
        if (rawText) node.children.push(mov_text(rawText));
        i += found ? found.index + found[0].length : rest.length;
        stack.pop();
      }
    }
  }
  return root;
}
function mov_text_of(node) {
  if (!node) return "";
  if (node.kind === "text") return node.text || "";
  var s = "";
  var i;
  var kids = node.children || [];
  for (i = 0; i < kids.length; i++) s += mov_text_of(kids[i]);
  return s;
}
function mov_walk(node, out) {
  var kids = node && node.children ? node.children : [];
  var i;
  for (i = 0; i < kids.length; i++) {
    if (kids[i].kind === "el") {
      out.push(kids[i]);
      mov_walk(kids[i], out);
    }
  }
}
function mov_class_ok(node, query) {
  var raw = "";
  var i;
  for (i = 0; i < node.attrs.length; i++) {
    if (String(node.attrs[i].name).toLowerCase() === "class") raw = node.attrs[i].value;
  }
  var have = raw.split(/\s+/);
  var need = String(query).split(/\s+/);
  var n, h, ok;
  for (n = 0; n < need.length; n++) {
    if (!need[n]) continue;
    ok = false;
    for (h = 0; h < have.length; h++) if (have[h] === need[n]) ok = true;
    if (!ok) return false;
  }
  return true;
}
var mov_native_gumbo = {
  parse: function (html) {
    var tree = mov_parse_html(html);
    return { document: tree, root: tree };
  },
  nodeName: function (node) { return node && node.kind === "el" ? String(node.name).toUpperCase() : (node ? node.name : ""); },
  nodeType: function (node) {
    if (!node) return 0;
    if (node.kind === "text") return 3;
    if (node.kind === "doc") return 9;
    return 1;
  },
  nodeChilds: function (node) { return node && node.children ? node.children : []; },
  nodeTextContent: function (node) { return mov_text_of(node); },
  nodeAttributes: function (node) { return node && node.attrs ? node.attrs : []; },
  findById: function (node, id) {
    var all = [];
    mov_walk(node, all);
    var i, a;
    for (i = 0; i < all.length; i++) {
      for (a = 0; a < all[i].attrs.length; a++) {
        if (all[i].attrs[a].name === "id" && all[i].attrs[a].value === id) return all[i];
      }
    }
    return null;
  },
  findByClassName: function (node, cls) {
    var all = [];
    var out = [];
    mov_walk(node, all);
    var i;
    for (i = 0; i < all.length; i++) if (mov_class_ok(all[i], cls)) out.push(all[i]);
    return out;
  },
  findByTagName: function (node, tag) {
    var all = [];
    var out = [];
    mov_walk(node, all);
    var want = String(tag).toLowerCase();
    var i;
    for (i = 0; i < all.length; i++) if (all[i].name === want) out.push(all[i]);
    return out;
  }
};

function mov_plain(value) {
  return String(value || "").replace(/<[^>]+>/g, " ").replace(/\s+/g, " ").replace(/^\s+|\s+$/g, "");
}
function mov_at(node, path) {
  var cur = node;
  var i;
  for (i = 0; i < path.length; i++) {
    if (!cur) return "";
    cur = cur["\xffkids"][path[i]];
  }
  if (!cur || cur["\xffvalue"] == null) return "";
  return String(cur["\xffvalue"]);
}
function mov_kid(node, name) {
  if (!node || !node["\xffkids"]) return null;
  return node["\xffkids"][name] || null;
}
function mov_truth(v) {
  return v === true || v === 1 || v === "1" || v === "true";
}
function mov_setting_label(raw, kind) {
  var value = mov_kid(raw, "value");
  var held = value ? value["\xffvalue"] : "";
  if (kind === "bool") return mov_truth(held) ? "ON" : "OFF";
  if (kind === "integer") {
    var unit = mov_at(raw, ["unit"]);
    var n = held == null || held === "" ? "0" : String(held);
    return unit ? n + " " + unit : n;
  }
  if (kind === "string") return held == null ? "" : String(held);
  if (kind === "multiopt") {
    var id = held == null ? "" : String(held);
    var opts = mov_kid(raw, "options");
    var node = opts ? mov_kid(opts, id) : null;
    var title = node ? mov_at(node, ["title"]) : "";
    return title || id;
  }
  if (kind === "info") return mov_plain(mov_at(raw, ["description"]));
  return "";
}
function mov_describe_setting(raw, title) {
  var kind = mov_at(raw, ["type"]);
  if (!kind) return null;
  return { kind: kind, title: title || kind, value: mov_setting_label(raw, kind) };
}
function mov_collect_box(box, prefix, into) {
  if (!box || into.length >= 24) return;
  var seen = [];
  function has(raw) {
    var i;
    for (i = 0; i < seen.length; i++) if (seen[i] === raw) return true;
    return false;
  }
  function add(raw, fallback) {
    if (!raw || has(raw) || into.length >= 24) return;
    var kind = mov_at(raw, ["type"]);
    if (!kind) return;
    var en = mov_kid(raw, "enabled");
    var flag = en ? en["\xffvalue"] : true;
    if (flag === false || flag === 0 || flag === "0" || flag === "false") return;
    seen.push(raw);
    var title = mov_plain(mov_at(raw, ["metadata", "title"]) || fallback || kind);
    if (kind === "info" && (!title || title === "info")) title = "Note";
    if (prefix) title = prefix + " / " + title;
    var row = mov_describe_setting(raw, title);
    if (!row) return;
    mov_setting_nodes.push(raw);
    into.push(row);
  }
  var order = box["\xfforder"] || [];
  var i;
  for (i = 0; i < order.length; i++) add(order[i], "");
  var names = [];
  var k;
  var kids = box["\xffkids"] || {};
  for (k in kids) names.push(k);
  for (i = 0; i < names.length; i++) add(kids[names[i]], names[i]);
}
function mov_page_settings(root) {
  var out = [];
  mov_setting_nodes = [];
  var model = mov_child(root, "model");
  mov_collect_box(mov_kid(model, "options"), "", out);
  var g = mov_global;
  var apps = g && mov_kid(mov_kid(g, "settings"), "apps");
  var nodes = apps && mov_kid(apps, "nodes");
  var ids = [];
  var id;
  if (nodes && nodes["\xffkids"]) {
    for (id in nodes["\xffkids"]) ids.push(id);
  }
  var a;
  for (a = 0; a < ids.length; a++) {
    var app = nodes["\xffkids"][ids[a]];
    var name = mov_plain(mov_at(app, ["metadata", "title"]) || ids[a]);
    mov_collect_box(mov_kid(app, "nodes"), name, out);
  }
  return out;
}
function mov_item_options(raw) {
  var box = mov_kid(raw, "options");
  var out = [];
  var raws = [];
  if (!box) return { list: out, raws: raws };
  var order = box["\xfforder"] || [];
  var i;
  for (i = 0; i < order.length && out.length < 12; i++) {
    var o = order[i];
    var kind = mov_at(o, ["type"]) || "action";
    var title = mov_plain(mov_at(o, ["metadata", "title"]) || "");
    var url = mov_at(o, ["url"]);
    if (!title && !url && kind !== "separator") continue;
    out.push({ title: title || kind, url: url, kind: kind });
    raws.push(o);
  }
  return { list: out, raws: raws };
}
function mov_visible(root) {
  var model = mov_child(root, "model");
  var nodes = mov_child(model, "nodes");
  var order = nodes["\xfforder"] || [];
  var items = [];
  var i;
  for (i = 0; i < order.length && items.length < 48; i++) {
    var raw = order[i];
    var type = mov_at(raw, ["type"]) || "item";
    var title = mov_plain(mov_at(raw, ["metadata", "title"]) || mov_at(raw, ["url"]) || "");
    var url = mov_at(raw, ["url"]);
    if (type === "separator") continue;
    if (!title && !url && type !== "action") continue;
    items.push({ raw: raw, title: title || "Item", url: url, type: type });
  }
  return items;
}
function mov_harvest(root) {
  var model = mov_child(root, "model");
  var err = mov_at(model, ["error"]);
  var type = mov_at(model, ["type"]);
  var title = mov_plain(mov_at(model, ["metadata", "title"]) || mov_at(root, ["metadata", "title"]));
  var vis = mov_visible(root);
  var items = [];
  mov_item_opt_nodes = [];
  var i;
  for (i = 0; i < vis.length; i++) {
    var opts = mov_item_options(vis[i].raw);
    mov_item_opt_nodes.push(opts.raws);
    items.push({ title: vis[i].title, url: vis[i].url, type: vis[i].type, options: opts.list });
  }
  return {
    title: title,
    items: items,
    error: type === "openerror" ? err : "",
    pageOptions: mov_page_settings(root)
  };
}
function mov_follow_redirect() {
  var last = "";
  var guard = 0;
  while (mov_nav_url && guard < 4) {
    var url = mov_nav_url;
    var opened = mov_nav_opened;
    mov_nav_url = "";
    mov_nav_opened = false;
    last = url;
    guard++;
    if (opened) continue;
    var root = mov_make();
    mov_page_root = root;
    try { mov_open_into(url, root); }
    catch (e) {
      np.set(mov_child(root, "model"), "type", "openerror");
      np.set(mov_child(root, "model"), "error", String(e && (e.message || e)));
    }
    __movianFlush();
  }
  return last;
}
function mov_multi_ids(raw) {
  var opts = mov_kid(raw, "options");
  var ids = [];
  var i;
  var k;
  if (!opts) return ids;
  var order = opts["\xfforder"] || [];
  for (i = 0; i < order.length; i++) {
    var name = order[i] && order[i]["\xffname"];
    if (name && String(name).charCodeAt(0) !== 255) ids.push(String(name));
  }
  if (ids.length) return ids;
  if (!opts["\xffkids"]) return ids;
  for (k in opts["\xffkids"]) ids.push(k);
  return ids;
}
function mov_apply_setting(raw, how) {
  var kind = mov_at(raw, ["type"]);
  var text = how == null ? "" : String(how);
  if (text.charAt(0) === "\x1f") {
    if (kind === "string") np.set(raw, "value", text.substring(1));
    return;
  }
  if (kind === "bool" && text === "toggle") {
    var held = mov_kid(raw, "value");
    var on = held && mov_truth(held["\xffvalue"]);
    np.set(raw, "value", on ? false : true);
    return;
  }
  if (kind === "integer" && (text === "inc" || text === "dec")) {
    var cur = parseInt(mov_at(raw, ["value"]), 10);
    var step = parseInt(mov_at(raw, ["step"]) || "1", 10);
    var min = parseInt(mov_at(raw, ["min"]), 10);
    var max = parseInt(mov_at(raw, ["max"]), 10);
    if (isNaN(cur)) cur = 0;
    if (isNaN(step) || !step) step = 1;
    cur += text === "dec" ? -step : step;
    if (!isNaN(min) && cur < min) cur = min;
    if (!isNaN(max) && cur > max) cur = max;
    np.set(raw, "value", cur);
    return;
  }
  if (kind === "multiopt" && (text === "inc" || text === "dec")) {
    var ids = mov_multi_ids(raw);
    if (!ids.length) return;
    var curId = mov_at(raw, ["value"]);
    var idx = 0;
    var i;
    for (i = 0; i < ids.length; i++) if (ids[i] === curId) idx = i;
    idx = text === "dec" ? idx - 1 : idx + 1;
    if (idx < 0) idx = ids.length - 1;
    if (idx >= ids.length) idx = 0;
    var opts = mov_kid(raw, "options");
    mov_fire(opts, "selectchild", opts["\xffkids"][ids[idx]]);
    return;
  }
  if (kind === "action" && text === "go") {
    var sink = mov_kid(raw, "eventSink");
    if (sink) mov_fire(sink, "action", "Activate");
  }
}
function mov_finish_page(redirected) {
  var page = mov_harvest(mov_page_root);
  page.redirected = redirected || "";
  page.mode = "page";
  return page;
}
function mov_wipe_dir(node) {
  if (!node) return;
  node["\xffkids"] = Object.create(null);
  node["\xfforder"] = [];
  node["\xffsubs"] = [];
}
function mov_open_into(url, root) {
  var hit = null;
  var m = null;
  var i;
  for (i = 0; i < mov_routes.length; i++) {
    if (!mov_routes[i].re) continue;
    mov_routes[i].re.lastIndex = 0;
    m = mov_routes[i].re.exec(url);
    if (m) { hit = mov_routes[i]; break; }
  }
  var model = mov_child(root, "model");
  mov_wipe_dir(mov_child(model, "nodes"));
  mov_wipe_dir(mov_child(model, "options"));
  if (!hit) {
    np.set(model, "type", "openerror");
    np.set(model, "error", "No route for " + url);
    return;
  }
  np.set(model, "type", "directory");
  np.set(model, "error", "");
  np.set(model, "loading", false);
  var args = [];
  for (i = 1; i < m.length; i++) args.push(m[i] == null ? "" : String(m[i]));
  hit.cb(root, true, args);
}

var Plugin = { id: "", path: "", manifest: "{}", config: {} };
var Core = {
  currentVersionInt: 700273,
  currentVersionString: "7.0.273",
  deviceId: "movian-ps5",
  storagePath: "/data/movian/state",
  sleep: function () {},
  resourceDestroy: function (res) {
    if (typeof res === "number" && mov_fds[res]) {
      var slot = mov_fds[res];
      if (slot.mode.indexOf("w") >= 0 || slot.mode.indexOf("a") >= 0) mov_write(slot.path, slot.buf);
      delete mov_fds[res];
    }
    if (res && res.node && res.fn) {
      var subs = res.node["\xffsubs"];
      var i;
      for (i = 0; i < subs.length; i++) if (subs[i] === res) { subs.splice(i, 1); break; }
    }
  }
};

function __movianReset() {
  mov_services = [];
  mov_routes = [];
  mov_proxy_pairs = [];
  mov_cache = {};
  mov_fds = {};
  mov_timers = [];
  mov_global = mov_make();
  mov_global["\xffname"] = "";
}

function __movianLoad(dir, manifest, source) {
  __movianReset();
  var ctrl;
  try { ctrl = JSON.parse(manifest); } catch (e) { return "plugin.json: " + e; }
  var id = String(ctrl.id || "plugin");
  var path = String(dir || "/");
  if (path.charAt(path.length - 1) !== "/") path += "/";
  Plugin = { id: id, path: path, manifest: String(manifest || "{}"), config: {} };
  Core.storagePath = path + ".state";
  try { mov_mkdirs(Core.storagePath); } catch (e2) {}
  var api = mov_source("legacy/api-v1.js");
  if (api == null) return "missing api-v1";
  var geval = eval;
  try { geval(api); } catch (e3) { return "api-v1: " + (e3 && (e3.stack || e3.message) || e3); }
  if (typeof showtime !== "undefined" && showtime.RichText) RichText = showtime.RichText;
  if (source && source.charCodeAt(0) === 0xFEFF) source = source.substring(1);
  try {
    var fn = new Function(source);
    fn.call(typeof plugin === "undefined" ? {} : plugin);
  } catch (e4) {
    return String(e4 && (e4.stack || e4.message) || e4);
  }
  __movianFlush();
  return "";
}

function __movianServices() { return mov_services; }

function __movianOpen(url) {
  var root = mov_make();
  mov_page_root = root;
  mov_nav_url = "";
  mov_nav_opened = false;
  var thrown = "";
  try { mov_open_into(String(url), root); }
  catch (e) { thrown = String(e && (e.stack || e.message) || e); }
  __movianFlush();
  mov_follow_redirect();
  var page = mov_harvest(mov_page_root);
  if (thrown && !page.error) page.error = thrown;
  page.redirected = "";
  page.mode = "page";
  return page;
}

function __movianSetting(index, how) {
  var raw = mov_setting_nodes[index];
  if (!raw || !mov_page_root) return { mode: "error", error: "no setting", title: "", items: [], pageOptions: [], redirected: "" };
  mov_nav_url = "";
  mov_nav_opened = false;
  try { mov_apply_setting(raw, how == null ? "" : String(how)); }
  catch (e) { return { mode: "error", error: String(e && (e.message || e)), title: "", items: [], pageOptions: [], redirected: "" }; }
  __movianFlush();
  var url = mov_follow_redirect();
  var page = mov_finish_page(url);
  return page;
}

function __movianUse(itemIndex, optIndex) {
  if (!mov_page_root) return { mode: "error", error: "no page", url: "", title: "", items: [], pageOptions: [], redirected: "" };
  var vis = mov_visible(mov_page_root);
  var item = vis[itemIndex];
  if (!item) return { mode: "error", error: "no item", url: "", title: "", items: [], pageOptions: [], redirected: "" };
  var node = item.raw;
  if (optIndex >= 0) {
    var raws = mov_item_opt_nodes[itemIndex] || [];
    node = raws[optIndex];
    if (!node) return { mode: "error", error: "no option", url: "", title: "", items: [], pageOptions: [], redirected: "" };
    var kind = mov_at(node, ["type"]) || "action";
    if (kind === "location") return { mode: "location", url: mov_at(node, ["url"]), title: "", items: [], pageOptions: [], redirected: "", error: "" };
    if (kind === "separator" || kind === "info") return { mode: "none", url: "", title: "", items: [], pageOptions: [], redirected: "", error: "" };
  }
  mov_nav_url = "";
  mov_nav_opened = false;
  try {
    var sink = mov_kid(node, "eventSink");
    if (sink) mov_fire(sink, "action", "Activate");
  } catch (e2) {
    return { mode: "error", error: String(e2 && (e2.message || e2)), url: "", title: "", items: [], pageOptions: [], redirected: "" };
  }
  __movianFlush();
  var gone = mov_follow_redirect();
  return mov_finish_page(gone);
}

__movianReset();
mov_log("movian-ps5-plugin-host ready");
