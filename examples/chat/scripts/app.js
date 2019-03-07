var io = new IO({
  'path': 'io',
  'port': 9001,
  'secure': false,
  'hostname': 'localhost'
})
var client_nickname;
var colors = ['Red', 'Green', 'Blue', 'Orange', 'Purple', 'Magenta', 'Teal', 'Brown', 'Olive', 'Navy', 'Grey'];
var color_index = 0;
var color_map = {};

io
  .on('open', function (event) {
    //console.log('APP:open', event);
    addLog('Successfully connected to the server ');
    io.send('login', { 'nickname': client_nickname });
  })
  .on('close', function (event) {
    //console.log('APP:close', event);
    addLog('Unable to connect to the server');
  })
  .on('message', function (command, payload) {
    //console.log('APP:message', command, payload);
    switch (command) {
      case 'welcome':
        payload = JSON.parse(payload);
        break;
      case 'user joined':
        payload = JSON.parse(payload);
        addLog(payload.nickname + ' joined');
        payload.participants > 1 ?
          addLog('There are ' + payload.participants + ' participants') :
          addLog('You are the only participant at the moment');
        break;
      case 'user left':
        payload = JSON.parse(payload);
        addLog(payload.nickname + ' left');
        payload.participants > 1 ?
          addLog('There are ' + payload.participants + ' participants') :
          addLog('You are the only participant at the moment');
        break;
      case 'new message':
        payload = JSON.parse(payload);
        var color = color_map[payload.uuid] || (color_map[payload.uuid] = colors[color_index++]);
        color_index >= colors.length && (color_index = 0);
        addMessage(payload.nickname, color, payload.text);
        break;
    }
  })
  .on('reopening', function () {
    //console.log('APP:reopening');
  })
  .on('openfail', function () {
    //console.log('APP:openfail');
  })
  .on('error', function (error) {
    //console.log('APP:error', error);
  })

window.addEventListener('DOMContentLoaded', function () {
  document.getElementById('nickname_request').addEventListener('submit', function (event) {
    event.preventDefault();
    if (client_nickname = this.elements['nickname_request_input'].value) {
      io.open()
      this.style.display = 'none';
      document.getElementById('new_message_input').focus();
    }
  });
  document.getElementById('new_message').addEventListener('submit', function (event) {
    event.preventDefault();
    var new_message = this.elements['new_message_input'].value;
    if (new_message) {
      this.elements['new_message_input'].value = '';
      io.send('new message', { 'text': new_message })
    }
  });
});

function addLog(text) {
  var info_text = document.createElement('div');
  info_text.className = 'log';
  info_text.innerHTML = text;
  document.getElementById('messages').appendChild(info_text);
  info_text.scrollIntoView();
}

function addMessage(username, usercolor, text) {
  var message_container = document.createElement('div');
  message_container.className = 'message';
  var message_username = document.createElement('span');
  message_username.className = 'username';
  message_username.innerHTML = username;
  message_username.style = 'color: ' + usercolor;
  message_container.appendChild(message_username);
  var message_text = document.createElement('span');
  message_text.className = 'text';
  message_text.innerHTML = text;
  message_container.appendChild(message_text);
  document.getElementById('messages').appendChild(message_container);
  message_container.scrollIntoView();
}