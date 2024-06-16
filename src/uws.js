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

const TR = new TextEncoder();

/**
 * @param {string|Uint8Array|ArrayBuffer|undefined} data
 * @returns {Uint8Array}
 */
function ensureUint8Array(data) {
  if (typeof data === "string") return TR.encode(data);
  return data && data instanceof Uint8Array ? data : new Uint8Array(data);
}

/**
 * @param {Uint8Array[]} chunks
 * @param {number} chunksLength
 * @returns {Uint8Array}
 */
function concatUint8Array(chunks, chunksLength) {
  if (chunks.length === 1) {
    return chunks[0];
  } else {
    const output = new Uint8Array(chunksLength);
    let offset = 0;
    for (let i = 0; i < chunks.length; i++) {
      const chunk = chunks[i];
      output.set(chunk, offset);
      offset += chunk.byteLength;
    }
    return output;
  }
}

const MAX_U8 = Math.pow(2, 8) - 1;
const MAX_U16 = Math.pow(2, 16) - 1;
const OPCODES = [0, 1, 2, 3, 4, 5, 6].map((opcode) =>
  Uint8Array.from([opcode])
);

module.exports.DeclarativeResponse = class DeclarativeResponse {
  /** @type {Uint8Array[]} */
  #instructions = [];
  #instructionsLength = 0;

  /**
   * @param {0|1|2|3|4|5|6} opcode
   */
  #appendOpCode(opcode) {
    this.#instructions.push(OPCODES[opcode]);
    this.#instructionsLength += 1;
  }

  #appendInstruction(data) {
    const bytes = ensureUint8Array(data);
    const length = bytes.byteLength;
    if (length > MAX_U8)
      throw new RangeError(`Text byte length ${length} greater than ${MAX_U8}`);
    this.#instructions.push(Uint8Array.from([length]));
    if (length) this.#instructions.push(bytes);
    this.#instructionsLength += 1 + length;
  }

  #appendData(data) {
    const bytes = ensureUint8Array(data);
    const length = bytes.byteLength;
    if (length > MAX_U16)
      throw new RangeError(
        `Data byte length ${length} greater than ${MAX_U16}`
      );
    this.#instructions.push(
      Uint8Array.from([length & 0xff, (length >> 8) & 0xff])
    );
    if (length) this.#instructions.push(bytes);
    this.#instructionsLength += 2 + length;
  }

  writeHeader(key, value) {
    this.#appendOpCode(1);
    this.#appendInstruction(key);
    this.#appendInstruction(value);
    return this;
  }

  writeBody() {
    this.#appendOpCode(2);
    return this;
  }

  writeQueryValue(key) {
    this.#appendOpCode(3);
    this.#appendInstruction(key);
    return this;
  }

  writeHeaderValue(key) {
    this.#appendOpCode(4);
    this.#appendInstruction(key);
    return this;
  }

  write(data) {
    this.#appendOpCode(5);
    this.#appendData(data);
    return this;
  }

  writeParameterValue(key) {
    this.#appendOpCode(6);
    this.#appendInstruction(key);
    return this;
  }

  end(data) {
    this.#appendOpCode(0);
    this.#appendData(data);
    return concatUint8Array(this.#instructions, this.#instructionsLength)
      .buffer;
  }
};
