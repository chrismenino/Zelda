(() => {
'use strict';
const $ = id => document.getElementById(id);
const start = $('start'), loading = $('loading'), message = $('loadMsg'), status = $('status');
const controls = [...document.querySelectorAll('[data-button]')];
let initialized = false, loadingStarted = false, syncInProgress = false, syncPending = false;
let held = new Set(), pads = new Set(), keyboard = new Set(), pointers = new Map();
let toastTimer = 0;
let gamepadId = null;
const keyMap = {ArrowUp:0,ArrowDown:1,ArrowLeft:2,ArrowRight:3, Shift:4,Enter:5,x:6,X:6,z:7,Z:7,s:8,S:8,a:9,A:9,c:10,C:10,v:11,V:11};
function toast(msg) {
  $('toast').textContent = msg;
  $('toast').classList.add('show');
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => $('toast').classList.remove('show'), 2500);
}
function pushButton(index, down) {
  if (!initialized) return;
  const fn = window.Module?._WebButton;
  if (typeof fn === 'function') fn(index, down ? 1 : 0);
}
function reconcile() {
  const next = new Set([...pointers.values(), ...pads, ...keyboard]);
  for (const n of held) if (!next.has(n)) pushButton(n, false);
  for (const n of next) if (!held.has(n)) pushButton(n, true);
  held = next;
  controls.forEach(btn => btn.classList.toggle('active', next.has(+btn.dataset.button)));
}
function setPointer(e, n) {
  if (n===null) pointers.delete(e.pointerId); else pointers.set(e.pointerId, n);
  reconcile();
}
controls.forEach(btn => {
  btn.addEventListener('pointerdown', e => {
    if (!initialized) return;
    e.preventDefault();
    btn.setPointerCapture(e.pointerId);
    setPointer(e, +btn.dataset.button);
    resumeAudio();
  });
  btn.addEventListener('pointermove', e => {
    if (!pointers.has(e.pointerId) || !btn.classList.contains('dir')) return;
    const el = document.elementFromPoint(e.clientX, e.clientY)?.closest('.dir');
    if (el?.closest('#dpad')) setPointer(e, +el.dataset.button);
  });
  for (const type of ['pointerup','pointercancel','lostpointercapture'])
    btn.addEventListener(type,e=>setPointer(e,null));
});
window.addEventListener('keydown', e => {
  if (e.target.tagName === 'INPUT') return;
  const n = keyMap[e.key];
  if (n!==undefined) {e.preventDefault();keyboard.add(n);reconcile();}
});
window.addEventListener('keyup', e => {const n = keyMap[e.key]; if(n!==undefined){ e.preventDefault(); keyboard.delete(n);reconcile(); }});
window.addEventListener('blur', () => { keyboard.clear();pointers.clear();reconcile(); autoSave(); });
function getPadButtons() {
  const gp = [...(navigator.getGamepads?.()||[])].find(g=>g&&g.connected);
  const n = new Set();
  if (!gp) {gamepadId=null;return n;}
  if (gp.id!==gamepadId) {gamepadId=gp.id;toast('Bluetooth controller connected');}
  const b = i => gp.buttons[i] && (gp.buttons[i].pressed || gp.buttons[i].value>0.5);
  if(b(0)) n.add(7); // Xbox A -> SNES B (lower physical button)
  if(b(1)) n.add(6); // Xbox B -> SNES A (right physical button)
  if(b(2)) n.add(9); // Xbox X -> SNES Y
  if(b(3)) n.add(8); // Xbox Y -> SNES X
  if(b(4)) n.add(10);
  if(b(5)) n.add(11);
  if(b(8)) n.add(4);
  if(b(9)) n.add(5);
  if(b(12)||gp.axes[1]<-.4) n.add(0);
  if(b(13)||gp.axes[1]>.4) n.add(1);
  if(b(14)||gp.axes[0]<-.4) n.add(2);
  if(b(15)||gp.axes[0]>.4) n.add(3);
  return n;
}
function pollPad(){
  const next=getPadButtons();
  if(next.size!==pads.size||[...next].some(i=>!pads.has(i))){pads=next;reconcile();}
  requestAnimationFrame(pollPad);
}
requestAnimationFrame(pollPad);
function resumeAudio() {
  for(const possible of [window.SDL?.audioContext, window.Module?.SDL2?.audioContext]) {
    if (possible?.state==='suspended') possible.resume().catch(()=>{});
  }
}
function sync(){
  if (!initialized || !window.FS) return;
  if (syncInProgress) {syncPending=true;return;}
  syncInProgress=true;
  window.FS.syncfs(false, err => {
    syncInProgress=false;
    if(err) {console.warn(err);toast('Save sync failed — export a backup');}
    if(syncPending){syncPending=false;sync();}
  });
}
function call(name) {
  if(!initialized || typeof window.Module?.[name]!=='function'){toast('Game still loading');return false;}
  window.Module[name]();
  return true;
}
function autoSave() {if(callQuiet('_WebSave')) sync();}
function callQuiet(name) {if(initialized && typeof Module[name]==='function'){Module[name]();return true;}return false;}
function buildBackup() {
  const files = {};
  const fs = window.FS;
  if(!fs) return;
  for(const name of fs.readdir('/saves')) {
    if(name==='.'||name==='..'||fs.isDir(fs.stat('/saves/'+name).mode)) continue;
    const bytes = fs.readFile('/saves/'+name);
    let out='';
    for(let i=0;i<bytes.length;i+=8192) out += String.fromCharCode(...bytes.slice(i,i+8192));
    files[name]=btoa(out);
  }
  const blob = new Blob([JSON.stringify({app:'flame-lantern-ios-web',format:1,files})],{type:'application/json'});
  const link = document.createElement('a'); link.href=URL.createObjectURL(blob);
  link.download='flame-lantern-save-'+new Date().toISOString().slice(0,10)+'.json';
  document.body.appendChild(link);link.click();link.remove();
  setTimeout(()=>URL.revokeObjectURL(link.href),2500);
  toast('Save backup exported');
}
$('import').addEventListener('change', async e => {
  const f=e.target.files?.[0];if(!f)return;
  try {
    if(f.size>4_000_000)throw Error('Backup is too large');
    const data=JSON.parse(await f.text());
    if(data.app!=='flame-lantern-ios-web'||!data.files||data.format!==1)throw Error('Not a Flame & Lantern save');
    const entries=Object.entries(data.files);
    if(entries.length>60)throw Error('Too many files');
    for(const [name,value] of entries){
      if(!/^[a-zA-Z0-9._-]+$/.test(name)||typeof value!=='string')throw Error('Invalid save file');
      const bin=atob(value);if(bin.length>2_000_000)throw Error('Invalid save file size');
      FS.writeFile('/saves/'+name,Uint8Array.from(bin,x=>x.charCodeAt(0)));
    }
    sync();toast('Imported — refresh the page to load your save');
  }catch(err){toast('Import failed: '+err.message);}finally{e.target.value='';}
});
$('settings').addEventListener('click',()=>{$('options').hidden=!$('options').hidden;});
$('close').addEventListener('click',()=>{$('options').hidden=true;});
document.querySelectorAll('[data-action]').forEach(btn=>btn.addEventListener('click',()=>{
  const action=btn.dataset.action;
  if(action==='save'){if(call('_WebSave')){sync();toast('Snapshot saved on device');}}
  if(action==='load'){if(call('_WebLoad'))toast('Snapshot loaded');}
  if(action==='lighting'){if(call('_WebToggleLight')){sync();toast('Lighting toggled');}}
  if(action==='brightness'){if(call('_WebCycleLight')){sync();toast('Night brightness changed');}}
  if(action==='pause'){if(call('_WebPause'))toast('Pause toggled');}
  if(action==='backup'){if(initialized){autoSave();buildBackup();}}
  $('options').hidden=true;
}));
// Save and persist repeatedly because Safari can terminate background tabs without a pagehide event.
setInterval(autoSave, 25000);
document.addEventListener('visibilitychange',()=>{if(document.hidden)autoSave();else resumeAudio();});
window.addEventListener('pagehide',autoSave);
// Once installed, the app can run offline after successful first caching.
if('serviceWorker' in navigator && location.protocol==='https:') navigator.serviceWorker.register('./sw.js').catch(console.warn);
window.Module = {
  canvas: $('canvas'),
  print: (...args)=>console.log(...args),
  printErr: (...args)=>console.error(...args),
  setStatus: text => {if(text){message.textContent=String(text);status.textContent='Loading…';}},
  monitorRunDependencies: left => { $('progress').style.width = left?Math.max(10,95-left*13)+'%':'95%'; },
  onAbort: what => {message.textContent='Could not start game: '+what;start.hidden=false;start.textContent='RETRY';status.textContent='Error';},
  postRun: [()=>{
    initialized=true;
    window.FS = window.Module.FS;
    $('progress').style.width='100%';
    loading.hidden=true;
    status.textContent='Playing';
    resumeAudio();
    toast('Tip: rotate your iPhone to landscape');
  }],
};
start.addEventListener('click',()=>{
  if(loadingStarted){location.reload();return;}
  loadingStarted=true;
  start.hidden=true;
  message.textContent='Compiling adventure in your browser…';
  status.textContent='Loading…';
  fetch('./game.js',{method:'HEAD'}).then(res=>{
    if(!res.ok) throw new Error('WebAssembly build missing');
    const script=document.createElement('script');
    script.src='./game.js';script.async=true;
    script.onerror=()=>{loadingStarted=false;start.hidden=false;message.textContent='Game engine missing. Publish the compiled site first.';};
    document.body.appendChild(script);
  }).catch(err=>{
    loadingStarted=false;start.hidden=false;start.textContent='TRY AGAIN';
    message.textContent='Not compiled yet. Follow README to publish the playable browser version.';
    status.textContent='Build needed';
    console.error(err);
  });
});
})();
