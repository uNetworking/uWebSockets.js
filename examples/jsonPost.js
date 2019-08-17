/* Simple example for reading Post body and parsing Json */

const uWS = require('../dist/uws.js')
const port = 9001
const app = uWS.App().post('/json', (res, req) => {

	/* onAborted needs to be set if doing anything async like reading body */
	res.onAborted(() => res.aborted = true)

	/* read the request body */
	readBody(res, body => {

		/* get body text as string */
		console.log('body text:', body.toString())

		/* parse json */
		try { var obj = JSON.parse(body) }
		catch(e) { console.log('json parse error') }
		console.log('json object:', obj)

		/* if response was aborted can't use it */
		if (res.aborted) return

		/* respond to request */
		if (!obj) return res.writeStatus('400').end('invalid json')
		res.end('success')
	})
}).listen(port, token => token && console.log('Listening to port ' + port))

/* Helper function for reading body */
function readBody(res, cb) {
	let body = Buffer.from([])
	res.onData((arrayBuffer, isLast) => {
		body = Buffer.concat([body, Buffer.from(arrayBuffer)])
		if (isLast) cb(body)
	})
}
