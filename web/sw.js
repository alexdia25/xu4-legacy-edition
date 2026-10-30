// Offline support: the game's files are cached on the first visit and
// served from the cache afterwards, refreshed from the network when online.
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
self.addEventListener('fetch', (e) => {
  if (e.request.method !== 'GET') return;
  e.respondWith(caches.match(e.request, { ignoreSearch: true })
    .then((hit) => hit || fetch(e.request)));
});
