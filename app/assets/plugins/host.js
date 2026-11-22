(function (global) {
    'use strict';
    var routes = [];
    var service;
    var inspectors = [];
    var itemHooks = [];
    var options = [];
    var notice = '';
    var consent = false;
    var loadOption = global.__movianLoadOption;
    var nativeHttp = global.__movianHttp;
    var nativeReadStore = global.__movianReadStore;
    var nativeSaveStore = global.__movianSaveStore;
    var nativeParseHtml = global.__movianParseHtml;
    var nativeImportTorrent = global.__movianImportTorrent;
    var loadZipModule = global.__movianLoadZipModule;
    var importedModules = Object.create(null);
    var stores = Object.create(null);

    function openStore(name) {
        name = String(name);
        if (!/^[A-Za-z0-9_.-]{1,63}$/.test(name) || name === '.' || name === '..')
            throw new Error('Invalid plugin store name');
        if (stores[name]) return stores[name].value;
        if (Object.keys(stores).length >= 8) throw new Error('Too many plugin stores');
        var raw = nativeReadStore(name), value = {};
        if (raw) {
            try { value = JSON.parse(raw) || {}; } catch (_) {}
        }
        if (typeof value !== 'object' || Array.isArray(value)) value = {};
        stores[name] = {value: value, original: JSON.stringify(value)};
        return value;
    }

    function finishPage(page) {
        Object.keys(stores).forEach(function (name) {
            var next = JSON.stringify(stores[name].value);
            if (next !== stores[name].original && !nativeSaveStore(name, next))
                throw new Error('Could not save plugin store');
        });
        return page;
    }

    function richText(value) {
        if (value && typeof value.toRichString === 'function')
            value = value.toRichString();
        var named = {amp: '&', quot: '"', apos: "'", lt: '<', gt: '>', nbsp: ' '};
        return String(value || '').replace(/<[^>]*>/g, '').replace(
            /&(#x[0-9a-f]+|#\d+|amp|quot|apos|lt|gt|nbsp);/ig,
            function (match, key) {
                if (key[0] !== '#') return named[key.toLowerCase()] || match;
                var hex = key[1].toLowerCase() === 'x';
                var code = parseInt(key.slice(hex ? 2 : 1), hex ? 16 : 10);
                return code > 0 && code <= 65535 ? String.fromCharCode(code) : match;
            });
    }
    global.RichText = function (value) { this.value = richText(value); };
    global.RichText.prototype.toString = function () { return this.value; };

    function parsedUrl(value) {
        var match = /^(https?:)\/\/([^/?#]+)([^#]*)/i.exec(value);
        if (!match) throw new Error('Invalid HTTP URL');
        var path = match[3] || '/';
        var q = path.indexOf('?');
        return {protocol: match[1], host: match[2], hostname: match[2],
                origin: match[1] + '//' + match[2], path: path,
                pathname: q < 0 ? path : path.slice(0, q), search: q < 0 ? '' : path.slice(q)};
    }

    function descendants(node, match, first) {
        var pending = (node.children || []).slice().reverse(), found = [];
        while (pending.length) {
            var child = pending.pop(), list = child.children || [];
            if (child.type === 1 && match(child)) {
                if (first) return new HtmlNode(child);
                found.push(new HtmlNode(child));
            }
            for (var i = list.length - 1; i >= 0; --i) pending.push(list[i]);
        }
        return first ? null : found;
    }

    function attribute(node, name) {
        var list = node.attributes || [];
        for (var i = 0; i < list.length; ++i)
            if (list[i].name === name) return list[i];
        return null;
    }

    function HtmlNode(node) { this._node = node; }
    Object.defineProperties(HtmlNode.prototype, {
        nodeName: {get: function () { return this._node.name.toLowerCase(); }},
        nodeType: {get: function () { return this._node.type; }},
        children: {get: function () {
            return (this._node.children || []).filter(function (node) { return node.type === 1; })
                .map(function (node) { return new HtmlNode(node); });
        }},
        attributes: {get: function () {
            var node = this._node, list = (node.attributes || []).slice();
            list.getNamedItem = function (name) { return attribute(node, String(name).toLowerCase()); };
            return list;
        }},
        textContent: {get: function () {
            if (this._node.text !== undefined) return this._node.text;
            var pending = (this._node.children || []).slice().reverse(), chunks = [];
            while (pending.length) {
                var node = pending.pop(), children = node.children || [];
                if (node.type === 3) chunks.push(node.text);
                for (var i = children.length - 1; i >= 0; --i) pending.push(children[i]);
            }
            return chunks.join('');
        }}
    });
    HtmlNode.prototype.getElementById = function (id) {
        return descendants(this._node, function (node) {
            var attr = attribute(node, 'id');
            return attr && attr.value === String(id);
        }, true);
    };
    HtmlNode.prototype.getElementByClassName = function (classes) {
        var wanted = String(classes).trim().split(/\s+/);
        if (!wanted[0]) return [];
        return descendants(this._node, function (node) {
            var attr = attribute(node, 'class');
            if (!attr) return false;
            var values = attr.value.split(/\s+/);
            return wanted.every(function (word) { return values.indexOf(word) >= 0; });
        }, false);
    };
    HtmlNode.prototype.getElementByTagName = function (tag) {
        tag = String(tag).toLowerCase();
        return descendants(this._node, function (node) {
            return tag === '*' || tag === node.name.toLowerCase();
        }, false);
    };
    HtmlNode.prototype.getElementsByClassName = HtmlNode.prototype.getElementByClassName;
    HtmlNode.prototype.getElementsByTagName = HtmlNode.prototype.getElementByTagName;

    function decodeCyrillic(bytes) {
        var map = 'ЂЃ‚ѓ„…†‡€‰Љ‹ЊЌЋЏђ‘’“”•–—�™љ›њќћџ ЎўЈ¤Ґ¦§Ё©Є«¬­®Ї°±Ііґµ¶·ё№є»јЅѕї';
        var out = '';
        for (var i = 0; i < bytes.length; ++i) {
            var ch = bytes[i];
            out += ch < 128 ? String.fromCharCode(ch)
                   : ch < 192 ? map[ch - 128] : String.fromCharCode(0x0410 + ch - 192);
        }
        return out;
    }

    function responseText(response, contentType) {
        var charset = /charset\s*=\s*["']?(?:windows-1251|cp1251)/i;
        return charset.test(contentType || '') || charset.test(response.body.slice(0, 3072))
            ? decodeCyrillic(response.bytes) : response.body;
    }

    function decodeUtf8(bytes) {
        var escaped = '', chunks = [];
        for (var i = 0; i < bytes.length; ++i) {
            var ch = typeof bytes === 'string' ? bytes.charCodeAt(i) : bytes[i];
            if (typeof ch !== 'number' || ch < 0 || ch > 255 || ch % 1)
                throw new Error('Expected byte data');
            escaped += '%' + ('0' + ch.toString(16)).slice(-2);
            if ((i & 1023) === 1023) {
                chunks.push(escaped);
                escaped = '';
            }
        }
        chunks.push(escaped);
        return decodeURIComponent(chunks.join(''));
    }

    function multiOption(id, title, choices, changed, items, type) {
        if (!choices || !choices.length) return;
        var stored = loadOption(id);
        function choiceKey(choice) {
            if (Plugin.id === 'KinoZal.TV' && id === 'domain') {
                if (choice[1] === 'Домен #1') return 'xdomain0';
                if (choice[1] === 'Домен #X') return 'xdomainx';
            }
            return String(choice[0]);
        }
        var index = 0;
        for (var i = 0; i < choices.length; ++i) {
            if (typeof stored === 'undefined' ? choices[i][2] :
                choiceKey(choices[i]) === stored || String(choices[i][0]) === stored) {
                index = i;
                break;
            }
        }
        if (changed) changed(choices[index][0]);
        if (items.length < 16)
            items.push({title: title, type: type,
                        url: id + '=' + choiceKey(choices[(index + 1) % choices.length]),
                        detail: String(choices[index][1])});
    }

    function query(values) {
        if (typeof values === 'string') return values;
        return Object.keys(values || {}).map(function (key) {
            return encodeURIComponent(key) + '=' + encodeURIComponent(String(values[key]));
        }).join('&');
    }

    function httpRequest(url, ctrl) {
        ctrl = ctrl || {};
        url = String(url);
        var args = query(ctrl.args);
        if (args) url += (url.indexOf('?') < 0 ? '?' : '&') + args;
        var headers = Object.create(null);
        var requestHeaders = ctrl.headers || {};
        Object.keys(requestHeaders).forEach(function (key) {
            headers[key.toLowerCase()] = String(requestHeaders[key]);
        });
        inspectors.forEach(function (entry) {
            if (entry.pattern.test(url)) entry.handler({
                url: url,
                setHeader: function (key, value) { headers[String(key).toLowerCase()] = String(value); }
            });
        });
        var post = String(ctrl.method || (ctrl.postdata ? 'POST' : 'GET')).toUpperCase();
        if (post !== 'GET' && post !== 'POST') throw new Error('Unsupported HTTP method');
        var body = post === 'POST' ? query(ctrl.postdata) : '';
        if (post === 'POST' && !headers['content-type'])
            headers['content-type'] = 'application/x-www-form-urlencoded';
        var flat = [];
        Object.keys(headers).forEach(function (key) { flat.push(key, headers[key]); });
        var response = nativeHttp(url, post === 'POST', flat, body);
        var original = parsedUrl(url);
        for (var hop = 0; !ctrl.noFollow && post === 'GET' && hop < 3; ++hop) {
            var location = /^location:\s*([^\r\n]+)/im.exec(response.headers);
            if (!/^(301|302|303|307|308)$/.test(response.status) || !location) break;
            var old = parsedUrl(url);
            var next = location[1].trim().split('#')[0];
            if (!next) break;
            if (next.indexOf('//') === 0) next = old.protocol + next;
            else if (next[0] === '/') next = old.origin + next;
            else if (next[0] === '?') next = old.origin + old.pathname + next;
            else if (!/^[a-z][a-z\d+.-]*:/i.test(next))
                next = old.origin + old.pathname.replace(/[^/]*$/, '') + next;
            if (!/^https:\/\//i.test(next) || next === url) break;
            url = next;
            response = nativeHttp(url, false, parsedUrl(url).origin === original.origin ? flat : [], '');
        }
        var all = [], map = Object.create(null), lower = Object.create(null);
        response.headers.split(/\r?\n/).forEach(function (line) {
            var colon = line.indexOf(':');
            if (colon < 1) return;
            var key = line.slice(0, colon), value = line.slice(colon + 1).trim();
            all.push(key, value);
            map[key] = value;
            lower[key.toLowerCase()] = value;
        });
        if (!ctrl.noFail && response.status >= 400) throw new Error('HTTP ' + response.status);
        return {statuscode: response.status, allheaders: all, headers: map, headers_lc: lower,
                bytes: response.bytes,
                toString: function () { return responseText(response, lower['content-type']); }};
    }

    var settings = {
        globalSettings: function () { return settings; },
        createInfo: function () {},
        createDivider: function () {},
        createAction: function (id, title) {
            if (Plugin.id === 'KinoZal.TV' && /_login$/.test(id))
                options.push({title: 'Sign in to KinoZal.TV', type: 'account',
                              url: id, detail: 'Session only · selected HTTPS domain'});
            else
                options.push({title: title, type: 'notice', detail: 'Action unavailable'});
        },
        createString: function (id, title, initial, changed) {
            var value = loadOption(id);
            if (typeof value === 'undefined') value = initial;
            if (changed) changed(value);
            if (id !== 'userCookie') options.push({title: title, type: 'text',
                                                url: id + '=', detail: String(value)});
        },
        createBool: function (id, title, initial, changed) {
            var current = loadOption(id);
            var value = typeof current === 'undefined' ? !!initial : current === '1';
            if (changed) changed(value);
            options.push({title: title, type: 'option', url: id + '=' + (value ? '0' : '1'),
                          detail: value ? 'On' : 'Off'});
        },
        createMultiOpt: function (id, title, choices, changed) {
            multiOption(id, title, choices, changed, options, 'option');
        }
    };

    var modules = {
        'movian/service': {
            create: function (title, url, type, enabled) {
                if (typeof type === 'undefined' &&
                    (typeof url === 'undefined' || typeof url === 'boolean'))
                    return openStore(title);
                if (!service && enabled) {
                    service = {title: title, url: url, type: type};
                    return service;
                }
            }
        },
        'movian/page': {
            Route: function (pattern, handler) {
                routes.push({pattern: new RegExp('^' + pattern + '$'), handler: handler});
            },
            Searcher: function () {}
        },
        'movian/settings': settings,
        'movian/store': {create: openStore},
        'movian/itemhook': {
            create: function (config) {
                if (itemHooks.length === 8 || typeof config.handler !== 'function')
                    throw new Error('Invalid plugin item action');
                var index = itemHooks.push(config) - 1;
                return {destroy: function () { itemHooks[index] = null; }};
            }
        },
        'native/string': {
            isUtf8: function (value) {
                try { decodeUtf8(value); return true; } catch (_) { return false; }
            },
            utf8FromBytes: function (value, encoding) {
                if (/^(windows-1251|cp1251)$/i.test(encoding || '')) {
                    if (typeof value === 'string')
                        value = value.split('').map(function (ch) { return ch.charCodeAt(0); });
                    return decodeCyrillic(value);
                }
                return decodeUtf8(value);
            }
        },
        'native/io': {
            httpInspectorCreate: function (pattern, handler) {
                if (inspectors.length < 8) inspectors.push({pattern: new RegExp(pattern), handler: handler});
            },
            httpReq: httpRequest
        },
        'movian/http': {request: httpRequest},
        'native/popup': {
            getAuthCredentials: function () { return null; },
            message: function (text, ok, question) {
                notice = String(text);
                consent = !!question;
                return false;
            }
        },
        'movian/prop': {},
        'movian/html': {parse: function (html) {
            if (!nativeParseHtml) throw new Error('HTML parser unavailable in this build');
            var document = new HtmlNode(nativeParseHtml(String(html)));
            return {document: document, root: document.getElementByTagName('html')[0]};
        }},
        'url': {parse: parsedUrl}
    };
    global.console = { log: function () {}, warn: function () {}, error: function () {} };
    function requireFrom(name, parent) {
        if (loadZipModule && /^\.\.?\//.test(name)) {
            var parts = parent.split('/');
            parts.pop();
            name.split('/').forEach(function (part) {
                if (!part || part === '.') return;
                if (part === '..') {
                    if (!parts.length) throw new Error('Plugin module path leaves its ZIP');
                    parts.pop();
                } else {
                    parts.push(part);
                }
            });
            var path = parts.join('/');
            if (!/\.js$/.test(path)) path += '.js';
            if (importedModules[path]) return importedModules[path].exports;
            if (Object.keys(importedModules).length >= 32) throw new Error('Too many ZIP modules');
            var source = loadZipModule(path);
            if (typeof source !== 'string') throw new Error('Unavailable ZIP module: ' + path);
            var module = importedModules[path] = {exports: {}};
            try {
                new Function('require', 'module', 'exports', source)(
                    function (child) { return requireFrom(child, path); }, module, module.exports);
            } catch (error) {
                delete importedModules[path];
                throw error;
            }
            return module.exports;
        }
        if (name.indexOf('showtime/') === 0)
            name = 'movian/' + name.slice(9);
        if (!Object.prototype.hasOwnProperty.call(modules, name))
            throw new Error('Unavailable plugin module: ' + name);
        return modules[name];
    }
    global.require = function (name) { return requireFrom(name, ''); };
    global.openPluginPage = function (url) {
        if (url === '#settings')
            return finishPage({title: 'Plugin settings', items: options});
        if (!service)
            throw new Error('Plugin did not register a service');
        url = url || service.url;
        var context = /^#hooks:([^:]+):([\s\S]*)$/.exec(url);
        if (context) {
            var actions = [];
            itemHooks.forEach(function (hook, index) {
                if (hook && String(hook.itemtype) === context[1])
                    actions.push({title: richText(hook.title), icon: String(hook.icon || ''),
                                  type: 'hook', url: '#hook:' + index + ':' + context[1] + ':' + context[2]});
            });
            return finishPage({title: 'Item actions', items: actions,
                               notice: actions.length ? '' : 'This plugin has no actions for this item'});
        }
        var action = /^#hook:(\d+):([^:]+):([\s\S]*)$/.exec(url);
        if (action) {
            var hook = itemHooks[Number(action[1])], destination;
            if (!hook || String(hook.itemtype) !== action[2])
                throw new Error('Plugin item action unavailable');
            hook.handler({metadata: {title: action[3]}}, {
                openURL: function (target) { destination = String(target); }
            });
            if (!destination || destination[0] === '#') throw new Error('Plugin item action has no route');
            url = destination;
        }
        var next = /^#page:([1-3]):([\s\S]+)$/.exec(url);
        var pageNumber = next ? Number(next[1]) : 0;
        if (next) url = next[2];
        for (var i = 0; i < routes.length; i++) {
            var match = routes[i].pattern.exec(url);
            if (!match)
                continue;
            var items = [];
            var more = false;
            var page = {
                type: 'directory',
                model: {},
                metadata: {title: service.title},
                options: {
                    createMultiOpt: function (id, title, choices, changed) {
                        var key = String(url) + '\0' + String(id), hash = 5381;
                        for (var j = 0; j < key.length; ++j)
                            hash = ((hash << 5) + hash) ^ key.charCodeAt(j);
                        multiOption('page-' + (hash >>> 0).toString(16), title, choices, changed, items, 'page-option');
                    }
                },
                error: function (text) { if (!notice) notice = String(text); },
                haveMore: function (available) { more = !!available; },
                appendItem: function (uri, type, metadata) {
                    if (items.length < 16) {
                        items.push({url: String(uri), type: String(type),
                                    title: richText(metadata && metadata.title || uri),
                                    icon: String(metadata && metadata.icon || ''),
                                    detail: richText(metadata && (metadata.description || metadata.synopsis) || '')});
                    }
                },
                appendPassiveItem: function (type, data, metadata) {
                    var title = metadata && metadata.title || data;
                    if (typeof title === 'string' && items.length < 16)
                        items.push({url: '', type: 'notice', title: title});
                },
                redirect: function (uri) { page.redirected = String(uri); }
            };
            routes[i].handler.apply(null, [page].concat(match.slice(1)));
            for (var index = 0; index < pageNumber; ++index) {
                if (!more || typeof page.asyncPaginator !== 'function')
                    throw new Error('Plugin page no longer available');
                items.length = 0;
                more = false;
                page.asyncPaginator();
            }
            if (more && pageNumber < 3 && typeof page.asyncPaginator === 'function') {
                if (items.length === 16) items.pop();
                items.push({url: '#page:' + (pageNumber + 1) + ':' + url, type: 'next',
                            title: 'Next page'});
            }
            if (consent) items.unshift({url: 'tosaccepted=1', type: 'consent', title: 'Accept plugin terms'});
            if (page.redirected) {
                var embedded = 'torrent:browse:data:application/x-bittorrent;base64,';
                if (page.redirected.indexOf(embedded) === 0 && nativeImportTorrent) {
                    var title = nativeImportTorrent(page.redirected.slice(embedded.length));
                    if (items.length === 16) items.pop();
                    items.push({url: '#torrent-file', type: 'torrent-file', title: title});
                } else {
                    items.push({url: page.redirected, type: 'link', title: 'Open link'});
                }
            }
            return finishPage({title: String(page.metadata.title), items: items, notice: notice});
        }
        throw new Error('Plugin route not found');
    };
})(this);
