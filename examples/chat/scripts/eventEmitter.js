/*
EventEmitter:
Event handling implementation:
 + Provides .on(), .off(), .once() and .emit() to classes who inherite from it.

Remarks:
 + Notation limited to ECMAScript 5 for compatibility reasons (IE 11...).
 + *this* is often stored into the variable *self* in order to obtain better minification.
*/

var EventEmitter;

(function () {
  'use strict';

  EventEmitter = function () {
    this._listeners = {};
  }

  EventEmitter.prototype.on = function (event, callback) {
    var self = this;
    (self._listeners[event] = self._listeners[event] || []).push(callback);
    return self;
  }

  EventEmitter.prototype.off = function (event, callback) {
    var self = this;
    // if no arguments: clear all listeners
    if (arguments.length === 0) {
      self._listeners = {};
      return self;
    }
    var listeners = self._listeners[event];
    if (!listeners) {
      return self;
    }
    // remove all callbacks for a specific event
    if (arguments.length === 1) {
      delete self._listeners[event];
      return self;
    }
    // remove a specific event/callback
    var i = listeners.indexOf(callback);
    if (~i) {
      listeners.splice(i, 1);
    }
    if (!listeners.length) {
      delete self._listeners[event];
    }
    return self;
  }

  EventEmitter.prototype.once = function (event, callback) {
    var self = this;
    function wrap() {
      self.off(event, wrap);
      callback.apply(this, arguments);
    }
    return self.on(event, wrap);
  }

  EventEmitter.prototype.emit = function (event) {
    var self = this;
    var listeners = self._listeners[event];
    if (!listeners) {
      return self;
    }
    for (var i = 0; i < listeners.length; i++) {
      var listener = listeners[i];
      switch (arguments.length) {
        // for 0 to 3 arguments
        case 1:
          listener();
          break;
        case 2:
          listener(arguments[1]);
          break;
        case 3:
          listener(arguments[1], arguments[2]);
          break;
        case 4:
          listener(arguments[1], arguments[2], arguments[3]);
          break;
      }
    }
    return self;
  }

})();