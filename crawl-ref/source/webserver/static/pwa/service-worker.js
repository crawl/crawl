const CACHE_NAME = "dcss-pwa-shell-v23";

const sw = /** @type {ServiceWorkerGlobalScope} */ (
  /** @type {unknown} */ (self)
);

// No precache list: shell assets are served by Tornado's static_url() with a
// ?v=<hash> query, so precaching bare paths never matches the runtime request.
// Runtime stale-while-revalidate below caches them on first visit instead.

sw.addEventListener("install", event => {
  event.waitUntil(sw.skipWaiting());
});

sw.addEventListener("activate", event => {
  event.waitUntil(
    caches.keys()
      .then(keys => Promise.all(
        keys.filter(key => key !== CACHE_NAME)
          .map(key => caches.delete(key))
      ))
      .then(() => sw.clients.claim())
  );
});

sw.addEventListener("fetch", event => {
  const url = new URL(event.request.url);

  if (event.request.method !== "GET" || url.origin !== sw.location.origin)
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
      .catch(() => caches.match(event.request)
        .then(response => response || Response.error()))
  );
});
