const uWS = require('../src/uws.js');

const app = uWS.App({
  // Optional SSL configuration
}).log((req, httpStatus, responseBody) => {
  console.log('HTTP parsing error occurred with status:', httpStatus);
  
  if (req) {
    console.log('Request URL:', req.getUrl());
    console.log('Request method:', req.getMethod());
    console.log('Request headers:', req.getHeader('host'));
  } else {
    console.log('Request information not available (parsing failed early)');
  }
  
  // Log the response body that will be sent to the client
  console.log('Response body length:', responseBody.byteLength);
  console.log('Response preview:', Buffer.from(responseBody).toString().substring(0, 100) + '...');
  
  // High-level program can now handle the error gracefully
  // The socket will be closed automatically after this handler runs
}).get('/*', (res, req) => {
  res.end('Hello World!');
}).listen(9001, (token) => {
  if (token) {
    console.log('Listening to port 9001 with HTTP parsing error handling');
  } else {
    console.log('Failed to listen to port 9001');
  }
});
