export default {
  build: {
    lib: {
      entry: "src/main.js",
      formats: ["iife"],
      name: "DCSSGame",
    },
    modulePreload: false,
    assetsDir: "static",
    rollupOptions: {
      input: "src/main.js",
      output: { entryFileNames: "static/game.js" },
      external: { jquery: "window.jQuery" },
    },
  },
};
