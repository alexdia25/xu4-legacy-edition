// Offline support: the game's files are cached on the first visit and used
// when the network is not there.
// @VERSION@ changes with every build, so a new build replaces the cache.
const CACHE = 'xu4-@VERSION@';
const FILES = ['./', 'index.html', 'xu4.js', 'xu4.wasm', 'xu4.data',
               'manifest.webmanifest', 'icon-192.png', 'icon-512.png'];

self.addEventListener('install', (e) => {
  e.waitUntil(caches.open(CACHE).then((c) => c.addAll(FILES)).then(() => self.skipWaiting()));
});
self.addEventListener('activate', (e) => {
  e.waitUntil(caches.keys().then((keys) =>
    Promise.all(keys.filter((k) => k !== CACHE).map((k) => caches.delete(k))))
    .then(() => self.clients.claim()));
});
// Online, everything comes from the network (the browser's own cache
// revalidates it), so a new build is used as soon as it is published and the
// page & game files always match; the cache is the fallback when offline.
self.addEventListener('fetch', (e) => {
  if (e.request.method !== 'GET') return;
  e.respondWith(fetch(e.request).catch(() =>
    caches.match(e.request, { ignoreSearch: true })
      .then((hit) => hit || caches.match('index.html'))));
});
