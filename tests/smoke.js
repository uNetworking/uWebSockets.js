// We are run inside tests folder and the newly built binaries are in ../dist
const uWS = require('../dist/uws.js');
const WebSocket = require('ws');

const expectedCloseReasons = {
  invalidFrame: 'Received invalid WebSocket frame',
  tcpFinBeforeClose: 'Received TCP FIN before WebSocket close frame',
  timeout: 'WebSocket timed out from inactivity',
};

let closeTestsPassed = true;

// Create a minimal uWebSockets.js server
const port = 9001;

const server = uWS.App().ws('/*', {
  idleTimeout: 8, // Set idle timeout to 8 seconds
  sendPingsAutomatically: false, // Needed for timeout
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
      } else if (reason === expectedCloseReasons.timeout) {
        console.log('Test passed: WebSocket timed out from inactivity');
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

    const triggerClosure = (malformedData, useEnd, useTimeout) => {
      const client = new WebSocket(`ws://localhost:${port}`);
      
      client.on('open', () => {
        console.log('Client connected to server');
        if (useEnd) {
          // Trigger "Received TCP FIN before WebSocket close frame"
          client._socket.end();
        } else if (useTimeout) {
          // Trigger timeout by keeping connection idle
          console.log('Starting timeout test: Keeping connection open for timeout');
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
    triggerClosure(malformedData, false, false);

    // Wait for Test 1 to complete before triggering the next test
    setTimeout(() => {
      // Test 2: Trigger TCP FIN before WebSocket close frame
      triggerClosure(null, true, false);
      
      // Wait for Test 2 to complete before starting Test 3
      setTimeout(() => {
        // Test 3: Trigger timeout (idleTimeout = 8 should close after ~32s)
        triggerClosure(null, false, true);
      }, 1000);
    }, 1000);

    // Allow time for all tests to complete before exiting
    setTimeout(() => {
      if (closeTestsPassed) {
        console.log('All tests passed.');
        process.exit(0);
      } else {
        console.error('Some tests failed.');
        process.exit(1);
      }
    }, 16000); // Increased to 40s to account for timeout test (~32s + buffer)
  } else {
    console.log('Failed to listen to port', port);
    process.exit(1);
  }
});
