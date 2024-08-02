const uWS = require('uWebSockets.js');
const WebSocket = require('ws');

const expectedCloseReasons = {
  invalidFrame: 'Received invalid WebSocket frame',
  tcpFinBeforeClose: 'Received TCP FIN before WebSocket close frame',
};

let closeTestsPassed = true;

// Create a minimal uWebSockets.js server
const port = 9001;

const server = uWS.App().ws('/*', {
  open: (ws) => {
    console.log('A WebSocket connected!');
  },
  message: (ws, message, isBinary) => {
    console.log('Received message:', message);
  },
  close: (ws, code, message) => {
    const reason = Buffer.from(message).toString();
    console.log('WebSocket closed with code:', code, 'and reason:', reason);
    
    if (code === 1006) {
      if (reason === expectedCloseReasons.invalidFrame) {
        console.log('Test passed: Invalid WebSocket frame');
      } else if (reason === expectedCloseReasons.tcpFinBeforeClose) {
        console.log('Test passed: TCP FIN before WebSocket close frame');
      } else {
        console.error('Test failed: Unexpected close reason:', reason);
        closeTestsPassed = false;
      }
    } else {
      console.error('Test failed: Unexpected close code:', code);
      closeTestsPassed = false;
    }
  }
}).listen(port, (token) => {
  if (token) {
    console.log('Listening to port', port);

    const triggerClosure = (malformedData, useEnd) => {
      const client = new WebSocket(`ws://localhost:${port}`);
      
      client.on('open', () => {
        console.log('Client connected to server');

        if (useEnd) {
          // Trigger "Received TCP FIN before WebSocket close frame"
          client._socket.end();
        } else {
          // Trigger "Received invalid WebSocket frame"
          client._socket.write(malformedData);
        }
      });

      client.on('close', (code, reason) => {
        console.log('Client WebSocket closed with code:', code, 'and reason:', reason);
      });

      client.on('error', (error) => {
        console.log('Client encountered error:', error);
      });
    };

    // Test 1: Trigger invalid WebSocket frame
    const malformedData = Buffer.from([0xFF, 0x80, 0x00, 0x00, 0x00, 0x01]);
    triggerClosure(malformedData, false);

    // Wait for a short period before triggering the next test to ensure the server handles sequentially
    setTimeout(() => {
      // Test 2: Trigger TCP FIN before WebSocket close frame
      triggerClosure(null, true);
    }, 1000); // Adjust the delay as necessary

    // Allow some time for tests to run before exiting
    setTimeout(() => {
      if (closeTestsPassed) {
        console.log('All tests passed.');
        process.exit(0);
      } else {
        console.error('Some tests failed.');
        process.exit(1);
      }
    }, 3000); // Adjust the delay as necessary
  } else {
    console.log('Failed to listen to port', port);
    process.exit(1);
  }
});
