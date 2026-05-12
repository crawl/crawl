const CACHE_NAME = "dcss-pwa-shell-v15";
const SHELL_ASSETS = [
  "/static/pwa/manifest.webmanifest",
  "/static/pwa/mobile-pwa.css",
  "/static/pwa/mobile-pwa.js",
  "/static/pwa/icons/apple-touch-icon.png",
  "/static/pwa/icons/icon-48.png",
  "/static/pwa/icons/icon-192.png",
  "/static/pwa/icons/icon-512.png"
];

self.addEventListener("install", event => {
  event.waitUntil(
    caches.open(CACHE_NAME)
      .then(cache => cache.addAll(SHELL_ASSETS))
      .then(() => self.skipWaiting())
  );
});

self.addEventListener("activate", event => {
  event.waitUntil(
    caches.keys()
      .then(keys => Promise.all(
        keys.filter(key => key !== CACHE_NAME)
          .map(key => caches.delete(key))
      ))
      .then(() => self.clients.claim())
  );
});

self.addEventListener("fetch", event => {
  const url = new URL(event.request.url);

  if (event.request.method !== "GET" || url.origin !== self.location.origin)
    return;

  if (!url.pathname.startsWith("/static/pwa/"))
    return;

  event.respondWith(
    fetch(event.request)
      .then(response => {
        const copy = response.clone();
        caches.open(CACHE_NAME).then(cache => cache.put(event.request, copy));
        return response;
      })
      .catch(() => caches.match(event.request))
  );
});
