// Isolated browser QA. Hardware access is never granted or requested.
import { spawn } from "node:child_process";
import { readFile, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { resolve } from "node:path";
const root = resolve(import.meta.dirname, "..");
const url = process.argv[2] || "http://127.0.0.1:8765/";
const profile = resolve(tmpdir(), "tamapoke-cyd-browser-" + Date.now());
const chrome = spawn(process.env.CYD_CHROME_BIN || "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome", [
  "--headless=new", "--disable-gpu", "--no-sandbox", "--no-first-run", "--no-default-browser-check",
  "--disable-background-networking", "--remote-debugging-port=0", `--user-data-dir=${profile}`, "about:blank",
], { stdio: "ignore" });
const pause = ms => new Promise(resolve => setTimeout(resolve, ms));
const checks = [], errors = []; let ws, serial = 0; const pending = new Map();
const mock = `export class Transport { async disconnect() {} }
export class ESPLoader {
  constructor() { this.chip={CHIP_NAME:window.__mockChip||'ESP32'}; }
  async main() { return this.chip.CHIP_NAME; }
  async readFlashId() { return 0x1640c8; }
  async readFlash(address,size) {
    if(address===0x8000) { if(window.__wrongPartition)return new Uint8Array(size);return new Uint8Array(await (await fetch(new URL('../../firmware/cyd-st7789/partitions.bin',import.meta.url))).arrayBuffer()); }
    return new Uint8Array(size).fill(3);
  }
  async writeFlash(options) {
    window.__writes.push({address:options.fileArray[0].address,bytes:options.fileArray[0].data.length,eraseAll:options.eraseAll,md5:options.calculateMD5Hash(options.fileArray[0].data)});
    options.reportProgress(0,50,100);await new Promise(r=>setTimeout(r,150));options.reportProgress(0,100,100);
  }
  async after() {}
}`;
try {
  let port;
  for (let i=0;i<100;i++) { try { port=Number((await readFile(profile+"/DevToolsActivePort","utf8")).split("\n")[0]);if(port)break; } catch {} await pause(100); }
  if (!port) throw new Error("Chrome did not start");
  const targets=await (await fetch(`http://127.0.0.1:${port}/json/list`)).json();
  ws=new WebSocket(targets.find(t=>t.type==="page").webSocketDebuggerUrl);
  await new Promise((resolve,reject)=>{ws.onopen=resolve;ws.onerror=reject;});
  const call=(method,params={})=>new Promise((resolve,reject)=>{const id=++serial;pending.set(id,{resolve,reject});ws.send(JSON.stringify({id,method,params}));});
  ws.onmessage=message=>{
    const data=JSON.parse(message.data);
    if(data.id){const p=pending.get(data.id);if(p){pending.delete(data.id);data.error?p.reject(data.error):p.resolve(data.result);}}
    if(data.method==="Runtime.exceptionThrown")errors.push(data.params.exceptionDetails.text);
    if(data.method==="Fetch.requestPaused")call("Fetch.fulfillRequest",{requestId:data.params.requestId,responseCode:200,responseHeaders:[{name:"Content-Type",value:"application/javascript"}],body:Buffer.from(mock).toString("base64")}).catch(e=>errors.push(String(e)));
  };
  const evaluate=async expression=>{const r=await call("Runtime.evaluate",{expression,returnByValue:true,awaitPromise:true});if(r.exceptionDetails)throw new Error(r.exceptionDetails.exception?.description||r.exceptionDetails.text);return r.result.value;};
  const wait=async expression=>{for(let i=0;i<150;i++){if(await evaluate(expression))return;await pause(100);}throw new Error("Timed out: "+expression);};
  const assert=(condition,message)=>{if(!condition)throw new Error(message);};
  await call("Page.enable");await call("Runtime.enable");await call("Network.enable");await call("Network.setCacheDisabled",{cacheDisabled:true});
  await call("Page.navigate",{url});
  await wait("document.getElementById('install-status')?.textContent.includes('選擇板型')");
  for(const width of [1280,390,320]) {
    await call("Emulation.setDeviceMetricsOverride",{width,height:1000,deviceScaleFactor:1,mobile:false});await pause(150);
    const layout=await evaluate("({overflow:document.documentElement.scrollWidth>innerWidth,missing:[...document.querySelectorAll('img')].filter(i=>!i.complete||!i.naturalWidth).map(i=>i.src)})");
    assert(!layout.overflow,"Horizontal overflow at "+width);
    if(width===1280||width===390){const shot=await call("Page.captureScreenshot",{format:"png"});await writeFile(root+`/docs/images/web-${width===1280?'desktop':'mobile'}.png`,Buffer.from(shot.data,"base64"));}
  }
  checks.push("real pinned libraries load under CSP", "responsive layout at 1280, 390 and 320 pixels");
  await call("Fetch.enable",{patterns:[{urlPattern:"*/vendor/esptool-js-0.6.0/bundle.js",requestStage:"Request"}]});
  const init=await call("Page.addScriptToEvaluateOnNewDocument",{source:"window.__writes=[];Object.defineProperty(navigator,'serial',{configurable:true,value:{requestPort:async()=>{if(window.__cancelPort)throw new DOMException('Cancelled','NotFoundError');return {};}}});"});
  await call("Page.navigate",{url:url+"?mock-qa=1"});await wait("document.getElementById('install-status')?.textContent.includes('選擇板型')");
  await evaluate("document.getElementById('board').value='cyd-st7789';document.getElementById('board').dispatchEvent(new Event('change'));document.getElementById('connect').click()");
  await wait("document.getElementById('install-status').textContent.includes('檢查通過')");
  assert(await evaluate("document.getElementById('flash').disabled&&document.getElementById('board').disabled&&window.__writes.length===0"),"consent gate and locked selection");
  await evaluate("document.getElementById('consent').checked=true;document.getElementById('consent').dispatchEvent(new Event('change'));document.getElementById('flash').click()");
  await wait("document.getElementById('install-status').textContent.includes('更新完成！')");
  assert(await evaluate("window.__writes.length===1&&window.__writes[0].address===65536&&window.__writes[0].eraseAll===false"),"app-only update boundary");
  checks.push("update consent and configuration locks", "mock update writes only app and verifies NVS");
  await evaluate("document.querySelector('input[value=factory]').checked=true;document.querySelector('input[value=factory]').dispatchEvent(new Event('change'));document.getElementById('connect').click()");
  await wait("document.getElementById('install-status').textContent.includes('檢查通過')");
  assert(await evaluate("document.getElementById('consent-label').textContent.includes('清除')&&document.getElementById('flash').disabled"),"factory confirmation");
  await evaluate("document.getElementById('consent').checked=true;document.getElementById('consent').dispatchEvent(new Event('change'));document.getElementById('flash').click()");
  await wait("document.getElementById('install-status').textContent.includes('安裝完成！')");
  assert(await evaluate("window.__writes.length===2&&window.__writes[1].address===0&&window.__writes[1].bytes===4194304"),"full factory boundary");
  checks.push("mock first installation requires erase acknowledgement");
  await evaluate("window.__mockChip='ESP32-S3';document.getElementById('connect').click()");
  await wait("document.getElementById('install-status').textContent.includes('其他晶片')");
  assert(await evaluate("window.__writes.length===2&&document.getElementById('flash').disabled"),"wrong chip");
  await evaluate("window.__mockChip='ESP32';window.__cancelPort=true;document.getElementById('connect').click()");
  await wait("document.getElementById('install-status').textContent.includes('已取消')");
  await evaluate("window.__cancelPort=false;window.__wrongPartition=true;document.querySelector('input[value=update]').checked=true;document.querySelector('input[value=update]').dispatchEvent(new Event('change'));document.getElementById('connect').click()");
  await wait("document.getElementById('install-status').textContent.includes('停止更新')");
  assert(await evaluate("window.__writes.length===2"),"mismatched partition writes nothing");
  checks.push("wrong chip stops before write", "cancelled picker recovers", "incompatible partitions stop update");
  await call("Page.removeScriptToEvaluateOnNewDocument",{identifier:init.identifier});await call("Fetch.disable");
  await call("Page.addScriptToEvaluateOnNewDocument",{source:"Object.defineProperty(navigator,'serial',{configurable:true,value:undefined});"});
  await call("Page.navigate",{url:url+"?unsupported-qa=1"});await wait("document.getElementById('install-status')?.textContent.includes('需使用')");
  assert(await evaluate("document.getElementById('connect').disabled&&document.getElementById('clock').disabled"),"unsupported browser controls");
  checks.push("unsupported browser guidance and disabled controls");
  assert(errors.length===0,"Browser errors: "+errors.join(";"));
  const report={recorded_at:new Date().toISOString(),url,checks,browser_exceptions:errors,hardware_flash_tested:false,hardware_scope:"Browser writes used a fake loader; physical firmware validation is recorded separately under evidence/cyd/.",vendor_source:"Real pinned libraries loaded in initial layout check; only device-facing ESPLoader was replaced for mock write flows."};
  await writeFile(root+"/evidence/web-validation.json",JSON.stringify(report,null,2)+"\n");console.log(JSON.stringify(report));
  await call("Browser.close");
} finally {if(ws)ws.close();chrome.kill();}
