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

module.exports = (() => {
	try {
		return require('./uws_' + process.platform + '_' + process.arch + '_' + process.versions.modules + '.node');
	} catch (e) {
		throw new Error('This version of uWS.js supports only Node.js LTS versions 16, 18 and 20 on (glibc) Linux, macOS and Windows, on Tier 1 platforms (https://github.com/nodejs/node/blob/master/BUILDING.md#platform-list).\n\n' + e.toString());
	}
})();

module.exports.DeclarativeResponse = class DeclarativeResponse {
  constructor() {
    this.instructions = [];
  }

  // Utility method to encode text and append instruction
  _appendInstruction(opcode, ...text) {
    this.instructions.push(opcode);
    text.forEach(str => {
      const bytes = new TextEncoder().encode(str);
      this.instructions.push(bytes.length, ...bytes);
    });
  }

  // Utility method to append 2-byte length text in little-endian format
  _appendInstructionWithLength(opcode, text) {
    this.instructions.push(opcode);
    const bytes = new TextEncoder().encode(text);
    const length = bytes.length;
    this.instructions.push(length & 0xff, (length >> 8) & 0xff, ...bytes);
  }

  writeHeader(key, value) { return this._appendInstruction(1, key, value), this; }
  writeBody() { return this.instructions.push(2), this; }
  writeQueryValue(key) { return this._appendInstruction(3, key), this; }
  writeHeaderValue(key) { return this._appendInstruction(4, key), this; }
  write(value) { return this._appendInstructionWithLength(5, value), this; }
  writeParameterValue(key) { return this._appendInstruction(6, key), this; }

  end(value) {
    const bytes = new TextEncoder().encode(value);
    const length = bytes.length;
    this.instructions.push(0, length & 0xff, (length >> 8) & 0xff, ...bytes);
    return new Uint8Array(this.instructions).buffer;
  }
}
