const VERSION = 'flame-lantern-iphone-v1';
const FILES = ['./','./index.html','./app.js','./style.css','./icon.svg','./manifest.webmanifest','./game.js','./game.wasm','./game.data'];
self.addEventListener('install', event => {
  event.waitUntil(caches.open(VERSION).then(cache=>cache.addAll(FILES).catch(err=>console.warn('Offline cache incomplete',err))));
  self.skipWaiting();
});
self.addEventListener('activate', event => {
  event.waitUntil(caches.keys().then(keys=>Promise.all(keys.filter(k=>k!==VERSION).map(k=>caches.delete(k)))));
  self.clients.claim();
});
self.addEventListener('fetch', event => {
  if (event.request.method!=='GET') return;
  event.respondWith(caches.match(event.request).then(hit=>hit||fetch(event.request).then(res=>{
    if (res.ok && new URL(event.request.url).origin===self.location.origin){let c=res.clone();caches.open(VERSION).then(cache=>cache.put(event.request,c));}
    return res;
  })));
});
