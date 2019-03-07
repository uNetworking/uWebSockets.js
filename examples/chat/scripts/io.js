/*
IO:
Wraps WebSockets in order to implement:
 + Keepalive with ping/pong.
 + Automatic reconnect.
 + Event based interface.

Remarks:
 + Notation limited to ECMAScript 5 for compatibility reasons (IE 11...).
 + *this* is often stored into the variable *self* in order to obtain better minification.
*/

var IO;

(function () {
  'use strict';

  IO = function (options) {
    //console.log('IO.constructor');
    var self = this;
    EventEmitter.call(self);
    self._hostname = window.location.hostname;
    self._port = 443;
    self._protocole = 'wss';
    self._path = '';
    self._reopenAttempts = 0;
    self._maxReopenAttempts = 30;
    self._keepAliveTimeout = 25000;
    self.set(options);
  }

  IO.prototype = Object.create(EventEmitter.prototype);

  IO.prototype.set = function (options) {
    //console.log('IO.set:', options);
    var self = this;
    typeof options.hostname === 'string' && (self._hostname = options.hostname);
    typeof options.port === 'number' && (self._port = options.port);
    typeof options.secure === 'boolean' && (self._protocole = (options.secure ? 'wss' : 'ws'));
    typeof options.path === 'string' && (self._path = options.path);
    typeof options.maxReopenAttempts === 'number' && (self._maxReopenAttempts = options.maxReopenAttempts);
    typeof options.keepAliveTimeout === 'number' && (self._keepAliveTimeout = options.keepAliveTimeout);
    //console.log(self);
    return self;
  }

  IO.prototype.open = function () {
    //console.log('IO.open');
    var self = this;
    self.websocket && self.websocket.close();
    self.websocket = new WebSocket(self._protocole + '://' + self._hostname + ':' + self._port + '/' + self._path);
    self.websocket.onopen = function (event) {
      clearTimeout(self._reopenTimeout);
      clearTimeout(self._pingTimeout);
      self._reopenAttempts = 0;
      self._pingTimeout = setTimeout(function () {
        self.send(''); // PING
      }, self._keepAliveTimeout);
      self.emit('open', event);
    };
    self.websocket.onclose = function (event) {
      self.emit('close', event);
      clearTimeout(self._pingTimeout);
      if (!self._clientRequestedClose && !event.wasClean) {
        // Close wasn't initiated by client nor server: Must reopen as long as max attempts hasn't been reached.
        if (self._reopenAttempts < self._maxReopenAttempts) {
          self.emit('reopening');
          self._reopenTimeout = setTimeout(function () {
            //console.log('IO: Reopen attempt');
            self.open();
          }, ++self._reopenAttempts * 1000);
        }
        else {
          self.emit('openfail');
        }
      }
    };
    self.websocket.onmessage = function (event) {
      if (event.data) { // Ignore PONG
        var indexOfComma = event.data.indexOf(',');
        indexOfComma >= 0 ?
          self.emit('message', event.data.slice(0, indexOfComma), event.data.slice(indexOfComma + 1)) :
          self.emit('message', event.data);
      }
    };
    self.websocket.onerror = function (error) {
      self.emit('error', error);
    };
    return self;
  }

  IO.prototype.close = function () {
    //console.log('IO.close');
    var self = this;
    self._clientRequestedClose = true;
    clearTimeout(self._reopenTimeout);
    clearTimeout(self._pingTimeout);
    self.websocket.close();
    return self;
  }

  IO.prototype.send = function (message, payload) {
    //console.log('IO.send', message, payload);
    var self = this;
    clearTimeout(self._pingTimeout);
    payload = JSON.stringify(payload);
    self.websocket.send(message + (payload ? ',' + payload : ''));
    self._pingTimeout = setTimeout(function () {
      self.send(''); // PING
    }, self._keepAliveTimeout);
    return self;
  }

})()
