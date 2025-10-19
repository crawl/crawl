export default {
  build: {
    lib: {
      entry: "src/main.js",
      formats: ["iife"],
      name: "DCSSGame",
    },
    modulePreload: false,
    assetsDir: "static",
    sourcemap: true,
    rollupOptions: {
      input: "src/main.js",
      output: {
        entryFileNames: "static/game.js",
        globals: {
          jquery: "jQuery",
          client: "DCSS.client",
          comm: "DCSS.comm",
          key_conversion: "DCSS.key_conversion",
        },
      },
      external: ["jquery", "client", "comm", "key_conversion"],
    },
  },
};
