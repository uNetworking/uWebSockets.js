/*
 * Authored by Alex Hultman, 2018-2024.
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
		throw new Error('This version of uWS.js (v20.52.0) supports only Node.js versions 20, 22, 23 and 24 on (glibc) Linux, macOS and Windows, on Tier 1 platforms (https://github.com/nodejs/node/blob/master/BUILDING.md#platform-list).\n\n' + e.toString());
	}
})();

const MAX_U8 = Math.pow(2, 8) - 1;
const textEncoder = new TextEncoder();
const toArrayBuffer = (value) => {
  if (typeof value === 'string') return textEncoder.encode(value);
  else if (!value) return new Uint8Array(0);
  else return value;
};

module.exports.DeclarativeResponse = class DeclarativeResponse {
  constructor() {
    this.instructions = [];
  }

  // Append instruction and 1-byte length values
  _appendInstruction(opcode, ...values) {
    this.instructions.push(opcode);
    values.forEach(value => {
      const arrayBuffer = toArrayBuffer(value);
      const length = arrayBuffer.length;
      if (length > MAX_U8) throw new RangeError('Data length exceeds ' + MAX_U8);
      this.instructions.push(length, ...arrayBuffer);
    });
  }

  // Append instruction and 4-byte length value
  _appendInstructionWithLength(opcode, value) {
    const arrayBuffer = toArrayBuffer(value);
    const lengthBuffer = new Buffer.alloc(4);
    lengthBuffer.writeUInt32LE(arrayBuffer.length);
    this.instructions.push(opcode, ...lengthBuffer, ...arrayBuffer);
  }

  writeHeader(key, value) { return this._appendInstruction(1, key, value), this; }
  writeBody() { return this.instructions.push(2), this; }
  writeQueryValue(key) { return this._appendInstruction(3, key), this; }
  writeHeaderValue(key) { return this._appendInstruction(4, key), this; }
  write(value) { return this._appendInstructionWithLength(5, value), this; }
  writeParameterValue(key) { return this._appendInstruction(6, key), this; }
  writeStatus(status) { return this._appendInstruction(7, status), this; }
  end(value) {
    this._appendInstructionWithLength(0, value);
    return new Uint8Array(this.instructions).buffer;
  }
}
