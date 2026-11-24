import { ESPLoader, Transport } from "./vendor/esptool-js-0.6.0/bundle.js";
import { FlashSession } from "./flasher.mjs";
import { synchronizeClock } from "./clock.mjs";

const $ = id => document.getElementById(id);
const secure = window.isSecureContext && !!navigator.serial;
let catalog, working = false, session;
const mode = () => document.querySelector('input[name="mode"]:checked').value;
function log(text) { $("logs").textContent = ($("logs").textContent + text + "\n").slice(-24000); }
function status(text, error = false) { $("install-status").textContent = text; $("install-status").dataset.error = String(error); log(text); }
function render() {
  const locked = working || session?.connected;
  $("board").disabled = !!locked;
  document.querySelectorAll('input[name="mode"]').forEach(input => input.disabled = !!locked);
  $("connect").disabled = !secure || !catalog || !$("board").value || !!locked;
  $("flash").disabled = working || !session?.prepared || !$("consent").checked;
  $("disconnect").disabled = working || !session?.connected;
  $("clock").disabled = !secure || working || !!session?.connected;
  $("consent").disabled = working;
  $("consent-label").textContent = mode() === "update" ? "確認目前已安裝此 CYD 版遊戲（0.1.1 以上）。" : "了解首次安裝會清除原程式、角色存檔與觸控校正。";
  $("flash").textContent = mode() === "update" ? "開始更新" : "開始安裝";
  const board = catalog?.boards.find(board => board.id === $("board").value);
  $("board-note").textContent = board?.id === "cyd-ili9341" ? "ILI9341 已完成編譯與主機測試，尚未實機驗證。請確認板上 LCD 規格。" : "適用 ESP32-2432S028、傳統 ESP32、4 MB Flash。LCD 版本請依板子實際規格確認。";
  for (const [id, key] of [["download-factory", "factory"], ["download-app", "app"]]) {
    $(id).hidden = !board; if (board) { $(id).href = board[key].path; $(id).download = ""; }
  }
}

function createSession() {
  return new FlashSession({ serial: navigator.serial, createLoader(port) {
    const transport = new Transport(port, false);
    const terminal = { clean() {}, writeLine: log, write: log };
    return { transport, loader: new ESPLoader({ transport, baudrate: 115200, terminal, debugLogging: false }) };
  }, md5: bytes => CryptoJS.MD5(CryptoJS.lib.WordArray.create(bytes)).toString(), status,
  progress(value) { $("progress").hidden = false; $("progress").value = Math.round(value * 100); } });
}

$("board").addEventListener("change", render);
document.querySelectorAll('input[name="mode"]').forEach(input => input.addEventListener("change", () => { $("consent").checked = false; render(); }));
$("consent").addEventListener("change", render);
$("connect").addEventListener("click", async () => {
  session = createSession(); working = true; render();
  try {
    // requestPort is reached immediately inside this user gesture.
    await session.connect();
    await session.prepare(catalog.boards.find(board => board.id === $("board").value), mode());
  } catch (error) {
    await session.close(); status(error.name === "NotFoundError" ? "已取消裝置選擇，可以再連接一次。" : error.message, error.name !== "NotFoundError");
  } finally { working = false; render(); }
});
$("disconnect").addEventListener("click", async () => {
  working = true; render(); await session.close(); working = false; status("已斷開，開發板會返回原本的遊戲。"); render();
});
$("flash").addEventListener("click", async () => {
  working = true; $("progress").value = 0; render();
  try {
    const result = await session.flash({ consent: $("consent").checked });
    status(result.nvsPreserved ? "更新完成！角色與校正比對一致。可點下方「連接並同步時間」。" : "安裝完成！先在開發板完成四點校正，再點下方「連接並同步時間」。");
  } catch (error) { status(error.message + " 請重新連接後再試。", true); }
  finally { working = false; $("consent").checked = false; render(); }
});
$("clock").addEventListener("click", async () => {
  working = true; render();
  try { await synchronizeClock({ serial: navigator.serial, status: text => $("clock-status").textContent = text }); }
  catch (error) { $("clock-status").textContent = error.name === "NotFoundError" ? "已取消裝置選擇。" : error.message; }
  finally { working = false; render(); }
});
window.addEventListener("beforeunload", event => { if (session?.busy) { event.preventDefault(); event.returnValue = ""; } });

try {
  const response = await fetch("./catalog.json", { cache: "no-store" });
  if (!response.ok) throw new Error("版本資訊讀取失敗。");
  catalog = await response.json();
  if (catalog.version !== "0.4.0-cyd" || catalog.boards.length !== 2) throw new Error("版本資訊不一致，請重新整理。");
  if (globalThis.CryptoJS?.MD5("abc").toString() !== "900150983cd24fb0d6963f7d28e17f72") throw new Error("校驗工具尚未載入，請重新整理。");
  status(secure ? "選擇板型後，點「連接並檢查」。" : "燒錄需使用電腦版 Chrome 或 Edge，並透過 HTTPS 或 localhost 開啟。");
} catch (error) { catalog = null; status(error.message, true); }
render();
