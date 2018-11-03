module.exports = (() => {
	try {
		const uWS = require(`./uws_${process.platform}_${process.versions.modules}.node`);
		/* We are not compatible with Node.js domain */
		process.nextTick = (f, ...args) => uWS.nextTick(f.apply(...args));
		return uWS;
	} catch (e) {
		throw new Error('This version of µWS is not compatible with your Node.js build.');
	}
})();
