export function utcEpoch(date = new Date()) { return Math.floor(date.getTime() / 1000); }
export function statusFields(line) {
  if (!line.startsWith("STATUS ")) return null;
  return Object.fromEntries(line.split(/\s+/).filter(value => value.includes("=")).map(value => value.split("=", 2)));
}

export class Lines {
  constructor(port) { this.port = port; this.queue = []; this.pending = null; this.closed = false; }
  async open() {
    await this.port.open({ baudRate: 115200, bufferSize: 8192 }); this.opened = true;
    await this.port.setSignals?.({ dataTerminalReady: false, requestToSend: false });
    this.reader = this.port.readable.getReader(); this.writer = this.port.writable.getWriter(); this.pump = this.read();
  }
  async read() {
    const decoder = new TextDecoder(); let buffer = "";
    try {
      while (!this.closed) {
        const { value, done } = await this.reader.read(); if (done) break;
        buffer += decoder.decode(value, { stream: true });
        let end;
        while ((end = buffer.indexOf("\n")) >= 0) {
          const line = buffer.slice(0, end).trim(); buffer = buffer.slice(end + 1); if (!line) continue;
          if (this.pending) { const receive = this.pending; this.pending = null; receive(line); }
          else { this.queue.push(line); if (this.queue.length > 100) this.queue.shift(); }
        }
        if (buffer.length > 4096) buffer = buffer.slice(-4096);
      }
    } catch (error) { if (!this.closed) this.error = error; }
  }
  async line(timeout = 250) {
    if (this.queue.length) return this.queue.shift();
    if (this.error) throw this.error;
    return new Promise(resolve => {
      const timer = setTimeout(() => { this.pending = null; resolve(""); }, timeout);
      this.pending = line => { clearTimeout(timer); resolve(line); };
    });
  }
  async request(command, accept, timeout = 10000) {
    const end = Date.now() + timeout; let next = 0;
    while (Date.now() < end) {
      if (Date.now() >= next) { await this.writer.write(new TextEncoder().encode(command + "\n")); next = Date.now() + 900; }
      const line = await this.line();
      if (line.startsWith("ERROR")) throw new Error("開發板拒絕校時，請確認已安裝 CYD 韌體。");
      if (accept(line)) return line;
    }
    throw new Error("等待開發板回覆逾時。請關閉其他序列埠視窗，重新連線。");
  }
  async close() {
    this.closed = true;
    try { await this.reader?.cancel(); } catch { /* Device unplugged. */ }
    try { await this.pump; } catch { /* Read loop already stopped. */ }
    try { this.reader?.releaseLock(); } catch { /* Lock already released. */ }
    try { this.writer?.releaseLock(); } catch { /* Lock already released. */ }
    if (this.opened) try { await this.port.close(); } catch { /* Already closed. */ }
  }
}

export async function synchronizeClock({ serial, date = new Date(), status = () => {}, Connection = Lines }) {
  if (!serial) throw new Error("請使用電腦版 Chrome 或 Edge。");
  const port = await serial.requestPort({ filters: [{ usbVendorId: 0x1a86 }, { usbVendorId: 0x10c4 }, { usbVendorId: 0x0403 }] });
  const connection = new Connection(port);
  try {
    await connection.open(); status("正在等待 CYD 啟動並辨識韌體。");
    const first = statusFields(await connection.request("STATUS", line => line.startsWith("STATUS ")));
    if (!first || !["cyd-st7789", "cyd-ili9341"].includes(first.variant) || !first.version?.endsWith("-cyd")) {
      throw new Error("這個裝置不是 TamaPoke CYD，未送出校時指令。");
    }
    await connection.request("TIME " + utcEpoch(date), line => line === "DONE");
    const final = statusFields(await connection.request("STATUS", line => statusFields(line)?.time_valid === "1"));
    status("校時完成，角色與進度已保留。"); return final;
  } finally { await connection.close(); }
}
