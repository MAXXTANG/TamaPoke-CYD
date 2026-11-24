import test from "node:test";
import assert from "node:assert/strict";
import { createRequire } from "node:module";
import { FlashSession, sha256, validateBoard, validateImage, APP_ADDRESS, NVS, PARTITIONS_ADDRESS } from "../web/flasher.mjs";
import { synchronizeClock, utcEpoch } from "../web/clock.mjs";
const require = createRequire(import.meta.url);
const CryptoJS = require("../web/vendor/crypto-js-4.2.0/crypto-js.js");
const md5 = bytes => CryptoJS.MD5(CryptoJS.lib.WordArray.create(bytes)).toString();

async function fixture({ chip = "ESP32", flashId = 0x1640c8, corrupt = false, mismatch = false, changedNvs = false } = {}) {
  const app = new Uint8Array(64); app[0] = 0xe9;
  const partitions = new Uint8Array(3072).fill(0xff);
  const factory = new Uint8Array(0x400000).fill(0xff);
  for (const offset of [0x1000, APP_ADDRESS]) { factory[offset] = 0xe9; factory[offset + 12] = factory[offset + 13] = 0; }
  const sources = { app, partitions, factory }, reads = [], writes = [];
  const board = { id: "cyd-st7789" };
  for (const [key, bytes] of Object.entries(sources)) board[key] = { path: key, bytes: bytes.length, sha256: await sha256(bytes) };
  let nvsReads = 0, requested = 0, closed = 0, resets = 0;
  const loader = {
    chip: { CHIP_NAME: chip }, async main() { return chip; }, async readFlashId() { return flashId; },
    async readFlash(address, size) {
      reads.push({ address, size });
      if (address === PARTITIONS_ADDRESS) return mismatch ? new Uint8Array(size) : partitions.slice();
      assert.equal(address, NVS.address); nvsReads++;
      return new Uint8Array(size).fill(changedNvs && nvsReads > 1 ? 4 : 3);
    },
    async writeFlash(options) { assert.equal(options.calculateMD5Hash(options.fileArray[0].data), md5(options.fileArray[0].data)); writes.push(options); },
    async after() { resets++; },
  };
  const session = new FlashSession({ serial: { async requestPort() { requested++; return {}; } },
    createLoader: () => ({ loader, transport: { async disconnect() { closed++; } } }), md5,
    fetcher: async path => ({ ok: true, async arrayBuffer() { return corrupt ? new Uint8Array(1).buffer : sources[path].slice().buffer; } }) });
  return { session, board, reads, writes, app, factory, get requested() { return requested; }, get closed() { return closed; }, get resets() { return resets; } };
}

test("MD5 typed-array and SHA-256 match independent known vectors", async () => {
  const bytes = new TextEncoder().encode("abc");
  assert.equal(md5(bytes), "900150983cd24fb0d6963f7d28e17f72");
  assert.equal(await sha256(bytes), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
});
test("wrong chip or flash size never writes", async () => {
  for (const options of [{ chip: "ESP32-S3" }, { flashId: 0x1740c8 }]) {
    const f = await fixture(options); await assert.rejects(f.session.connect());
    assert.equal(f.writes.length, 0); assert.equal(f.closed, 1);
  }
  assert.throws(() => validateBoard("ESP8266", 0x1640c8));
});
test("corrupt download or incompatible partitions prevents every write", async () => {
  for (const options of [{ corrupt: true }, { mismatch: true }]) {
    const f = await fixture(options); await f.session.connect();
    await assert.rejects(f.session.prepare(f.board, "update")); assert.equal(f.writes.length, 0);
    await f.session.close();
  }
});
test("update writes only app, never erases, and verifies NVS on both sides", async () => {
  const f = await fixture(); await f.session.connect(); await f.session.prepare(f.board, "update");
  const result = await f.session.flash({ consent: true });
  assert.equal(result.nvsPreserved, true); assert.equal(f.requested, 1); assert.equal(f.closed, 1);
  assert.equal(f.writes.length, 1); assert.equal(f.writes[0].fileArray[0].address, 0x10000);
  assert.equal(f.writes[0].eraseAll, false); assert.equal(f.writes[0].flashSize, "keep");
  assert.deepEqual(f.reads.map(read => read.address), [0x8000, 0x9000, 0x9000]);
});
test("factory install requires consent and writes the complete image at zero", async () => {
  const f = await fixture(); await f.session.connect(); await f.session.prepare(f.board, "factory");
  await assert.rejects(f.session.flash(), /確認/); assert.equal(f.writes.length, 0);
  const result = await f.session.flash({ consent: true });
  assert.equal(result.nvsPreserved, false); assert.equal(f.writes[0].fileArray[0].address, 0);
  assert.equal(f.writes[0].fileArray[0].data.length, 4194304); assert.equal(f.reads.length, 0);
});
test("mutated buffer and altered NVS cannot be reported as a successful update", async () => {
  const f = await fixture(); await f.session.connect(); await f.session.prepare(f.board, "update");
  f.session.prepared.bytes[1] = 7;
  await assert.rejects(f.session.flash({ consent: true }), /已變更/); assert.equal(f.writes.length, 0);
  const g = await fixture({ changedNvs: true }); await g.session.connect(); await g.session.prepare(g.board, "update");
  await assert.rejects(g.session.flash({ consent: true }), /存檔區比對/); assert.equal(g.closed, 1);
});
test("image family and mode are validated", () => {
  const bytes = new Uint8Array(64); bytes[0] = 0xe9; bytes[12] = 9;
  assert.throws(() => validateImage(bytes, "update")); assert.throws(() => validateImage(bytes, "factory"));
  assert.throws(() => validateImage(bytes, "bogus"));
});
test("clock sync uses UTC epoch and refuses unknown firmware before TIME", async () => {
  const sent = []; let closed = false;
  class Connection {
    async open() {} async close() { closed = true; }
    async request(command) { sent.push(command); return "STATUS version=0.3.3 variant=tra-board"; }
  }
  await assert.rejects(synchronizeClock({ serial: { requestPort: async () => ({}) }, Connection }), /未送出/);
  assert.deepEqual(sent, ["STATUS"]); assert.equal(closed, true);
  assert.equal(utcEpoch(new Date("2026-11-24T22:00:00+08:00")), 1795528800);
});
test("known CYD clock sync sends no pet selection or erase command", async () => {
  const sent = [];
  class Connection {
    async open() {} async close() {}
    async request(command) { sent.push(command); return command.startsWith("TIME ") ? "DONE" : "STATUS version=0.4.0-cyd variant=cyd-st7789 time_valid=1"; }
  }
  const fields = await synchronizeClock({ serial: { requestPort: async () => ({}) }, Connection, date: new Date("2026-11-24T22:00:00+08:00") });
  assert.equal(fields.time_valid, "1"); assert.deepEqual(sent, ["STATUS", "TIME 1795528800", "STATUS"]);
});
