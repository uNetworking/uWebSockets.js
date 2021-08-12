/*
 * Authored by Alex Hultman, 2018-2020.
 * Intellectual property of third-party.

 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at

 *     http://www.apache.org/licenses/LICENSE-2.0

 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

module.exports = (() => {
	if (
		process.env.UWS_USE_FALLBACK === "true" ||
		process.env.UWS_USE_FALLBACK === "True" ||
		process.env.UWS_USE_FALLBACK === "TRUE" ||
		process.env.UWS_USE_FALLBACK === "1"
	) {
		console.log("Using uWS fallback implementation due to UWS_USE_FALLBACK environment flag being enabled; performance may be degraded.\n\n");
		const fallback = require("../dist/index");
		return fallback;
	}

	try {
		const uWS = require('../binaries/uws_' + process.platform + '_' + process.arch + '_' + process.versions.modules + '.node');
		if (process.env.EXPERIMENTAL_FASTCALL) {
			process.nextTick = (f, ...args) => {
				Promise.resolve().then(() => {
					f(...args);
				});
			};
		}
		return uWS;
	} catch (e) {
		console.warn('This version of µWS is not compatible with your Node.js build:\n\n' + e.toString());
		console.warn("Falling back to an NodeJS implementation; performance may be degraded.\n\n");
		const fallback = require("../dist/index");
		return fallback;
	}
})();
