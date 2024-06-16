/* Non-SSL is simply App() */
const uWS = require('uWebSockets.js');
	uWS.App()
	.get('/', new uWS.DeclarativeResponse().writeHeader('content-type', 'text/plain').end('Hi'))
	.get('/favicon.ico',new uWS.DeclarativeResponse().writeHeader('content-type', 'image/x-icon').end(require('node:fs').readFileSync('./favicon.ico')))
	.get('/id/:id', new uWS.DeclarativeResponse().writeHeader('content-type', 'text/plain')
					.writeHeader('x-powered-by', 'benchmark')
					.writeParameterValue("id")
					.write(" ")
					.writeQueryValue("name")
					.end())
	.post('/json', (res, req) => {
		readJson(
			res,
			(obj) => {
				res.writeHeader('content-type', 'application/json').end(
					JSON.stringify(obj)
				)
			},
			() => {
				res.end('Ok')
			}
		)
	})
	.listen(3000, (listenSocket) => {
		if (listenSocket) {
			console.log('Listening to port 3000')
		}
	})

function readJson(res, cb, err) {
	let buffer

	res.onData((ab, isLast) => {
		let chunk = Buffer.from(ab)
		if (isLast) {
			if (buffer) {
				cb(JSON.parse(Buffer.concat([buffer, chunk])))
			} else {
				cb(JSON.parse(chunk))
			}
		} else {
			if (buffer) {
				buffer = Buffer.concat([buffer, chunk])
			} else {
				buffer = Buffer.concat([chunk])
			}
		}
	})

	res.onAborted(err)
}
