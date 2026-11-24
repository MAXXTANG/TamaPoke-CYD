export const FLASH_BYTES = 0x400000;
export const NVS = { address: 0x9000, bytes: 0x5000 };
export const PARTITIONS_ADDRESS = 0x8000;
export const APP_ADDRESS = 0x10000;

export async function sha256(bytes) {
  return [...new Uint8Array(await crypto.subtle.digest("SHA-256", bytes))]
    .map(value => value.toString(16).padStart(2, "0")).join("");
}

export function validateBoard(chip, flashId) {
  if (chip !== "ESP32") throw new Error("這個韌體適用傳統 ESP32。偵測到其他晶片，已停止燒錄。");
  if (((flashId >>> 16) & 255) !== 0x16) throw new Error("需要 4 MB Flash 的 CYD；偵測到容量不同，已停止燒錄。");
}

export async function verifiedAsset(file, fetcher = fetch) {
  const response = await fetcher(file.path, { cache: "no-store" });
  if (!response.ok) throw new Error("韌體下載失敗，請稍後重新整理。");
  const bytes = new Uint8Array(await response.arrayBuffer());
  if (bytes.length !== file.bytes || await sha256(bytes) !== file.sha256) {
    throw new Error("韌體完整性檢查未通過，已停止燒錄。請重新整理後再試。");
  }
  return bytes;
}

function classicImage(bytes, offset) {
  return bytes[offset] === 0xe9 && bytes[offset + 12] === 0 && bytes[offset + 13] === 0;
}

export function validateImage(bytes, mode) {
  if (mode === "update") {
    if (!bytes.length || bytes.length > 0x300000 || !classicImage(bytes, 0)) throw new Error("更新映像格式不正確。");
  } else if (mode === "factory") {
    if (bytes.length !== FLASH_BYTES || !classicImage(bytes, 0x1000) || !classicImage(bytes, APP_ADDRESS)) {
      throw new Error("首次安裝映像格式不正確。");
    }
  } else throw new Error("請選擇安裝方式。");
}

export class FlashSession {
  constructor({ serial, createLoader, md5, fetcher = fetch, status = () => {}, progress = () => {} }) {
    this.serial = serial; this.createLoader = createLoader; this.md5 = md5; this.fetcher = fetcher;
    this.status = status; this.progress = progress; this.connected = false; this.busy = false;
  }

  async connect() {
    if (!this.serial) throw new Error("請使用電腦版 Chrome 或 Edge，透過 HTTPS 開啟此頁。");
    if (this.connected || this.busy) throw new Error("已有連線，請先斷開再切換。");
    const port = await this.serial.requestPort({ filters: [{ usbVendorId: 0x1a86 }, { usbVendorId: 0x10c4 }, { usbVendorId: 0x0403 }] });
    this.busy = true;
    try {
      const { loader, transport } = this.createLoader(port);
      this.loader = loader; this.transport = transport;
      this.status("正在辨識晶片與 Flash 容量，遊戲會暫時重新啟動。");
      const description = await loader.main();
      validateBoard(loader.chip?.CHIP_NAME, await loader.readFlashId());
      this.connected = true;
      this.status("已確認 ESP32 / 4 MB。正在檢查韌體。");
      return description;
    } catch (error) { await this.close(); throw error; }
    finally { this.busy = false; }
  }

  async prepare(board, mode) {
    if (!this.connected || this.busy) throw new Error("請先連接開發板。");
    this.prepared = null; this.busy = true;
    try {
      if (!board || !["cyd-st7789", "cyd-ili9341"].includes(board.id)) throw new Error("請選擇 CYD 板型。");
      const image = mode === "factory" ? board.factory : mode === "update" ? board.app : null;
      if (!image) throw new Error("安裝方式不正確。");
      const bytes = await verifiedAsset(image, this.fetcher); validateImage(bytes, mode);
      if (mode === "update") {
        const expected = await verifiedAsset(board.partitions, this.fetcher);
        const current = await this.loader.readFlash(PARTITIONS_ADDRESS, expected.length);
        if (current.length !== expected.length || await sha256(current) !== board.partitions.sha256) {
          throw new Error("板上的分割表與 CYD 0.1.1 以上版本不同，已停止更新。請先備份，並依安裝說明操作。");
        }
      }
      this.prepared = { board, mode, bytes, sha256: image.sha256, address: mode === "factory" ? 0 : APP_ADDRESS };
      this.status(mode === "update" ? "檢查通過，更新只寫入程式區，角色與校正會保留。" : "檢查通過。首次安裝會清除板上的原程式、角色與校正。");
    } finally { this.busy = false; }
  }

  async flash({ consent = false } = {}) {
    if (!this.prepared || !this.connected || this.busy) throw new Error("請先完成連線與韌體檢查。");
    if (!consent) throw new Error("請先確認所選的安裝方式。");
    if (typeof this.md5 !== "function") throw new Error("韌體校驗工具未載入，已停止燒錄。");
    const task = this.prepared; this.busy = true;
    try {
      // Recheck the buffer at the write boundary, including after any UI delay.
      if (await sha256(task.bytes) !== task.sha256) throw new Error("待寫入資料已變更，已停止燒錄。");
      validateImage(task.bytes, task.mode);
      const before = task.mode === "update" ? await this.loader.readFlash(NVS.address, NVS.bytes) : null;
      if (before && before.length !== NVS.bytes) throw new Error("無法讀取完整存檔區，已停止更新。");
      this.status("正在寫入並校驗韌體，請保持 USB 連線。");
      await this.loader.writeFlash({
        fileArray: [{ data: task.bytes, address: task.address }],
        flashMode: "keep", flashFreq: "keep", flashSize: "keep", eraseAll: false, compress: true,
        calculateMD5Hash: this.md5,
        reportProgress: (_file, written, total) => this.progress(total ? written / total : 0),
      });
      if (before) {
        const after = await this.loader.readFlash(NVS.address, NVS.bytes);
        if (after.length !== before.length || await sha256(after) !== await sha256(before)) {
          throw new Error("程式已寫入，但存檔區比對未通過。請保留開發板，依進階備份說明檢查。");
        }
      }
      const result = { variant: task.board.id, mode: task.mode, sha256: task.sha256, nvsPreserved: !!before };
      this.progress(1); this.status(before ? "更新完成，存檔與觸控校正比對一致。" : "安裝完成。請在螢幕依序完成四點觸控校正。");
      return result;
    } finally { this.busy = false; await this.close(); }
  }

  async close() {
    try { if (this.loader?.chip) await this.loader.after("hard_reset"); }
    catch { /* A failed connection may not have a working reset path. */ }
    try { await this.transport?.disconnect(); } catch { /* Already disconnected. */ }
    this.connected = false; this.prepared = null; this.loader = null; this.transport = null;
  }
}
