/* Non-SSL is simply App() */
import uWS from 'uWebSockets.js';
	uWS.App()
	.get('/', new uWS.DeclarativeResponse().writeHeader('content-type', 'text/plain').end('Hi'))
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
				/* Aborted or invalid JSON, res must not be used here */
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
			let json
			try {
				json = JSON.parse(buffer ? Buffer.concat([buffer, chunk]) : chunk)
			} catch (e) {
				/* res.close calls onAborted */
				res.close()
				return
			}
			cb(json)
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
