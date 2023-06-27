/*
 * Authored by Alex Hultman, 2018-2022.
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
const { existsSync } = require('node:fs');
const { arch, platform, versions: { modules } } = require('node:rocess');

try {
	if (modules == '93' || modules == '108' || modules == '115')
		if (platform == 'win32')
			if (arch == 'x64')
				module
					.exports = require(`./uws_win32_x64_${modules}.node`);
			else
				throw new Error('This version of uWebSockets.js only supports x64 architecture on Windows.');
		else if (platform == 'linux')
			if (arch == 'x64' || arch == 'arm' || arch == 'arm64')
				module
					.exports = require(`./uws_linux_${arch}_${modules}.node`);
			else
				throw new Error('This version of uWebSockets.js only supports x64, arm or arm64 architecture on Linux.');
		else if (platform == 'darwin')
			if (arch == 'x64' || arch == 'arm64')
				module
					.exports = require(`./uws_darwin_${arch}_${modules}.node`);
			else
				throw new Error('This version of uWebSockets.js only supports x64 or arm64 architecture on macOS.');
		else
			throw new Error('This version of uWebSockets.js only supports (glibc) Linux, macOS and Windows systems.');
	else
		throw new Error('This version of uWebSockets.js only supports Node.js LTS versions 16, 18 and 20.');
} catch {
	if (existsSync(`./uws_${platform}_${arch}_${modules}.node`))
		throw new Error('You are missing some required files for uWebSockets.js. Please try installing it again.');
	else
		throw new Error('You are missing some dependencies for uWebSockets.js. Please try installing Node.js or Visual Studio build tools.');
};