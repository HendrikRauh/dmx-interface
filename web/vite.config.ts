import preact from "@preact/preset-vite";
import { defineConfig } from "vite";
import { mockDevServerPlugin } from "vite-plugin-mock-dev-server";
import { viteSingleFile } from "vite-plugin-singlefile";

// https://vitejs.dev/config/
export default defineConfig({
  plugins: [
    preact(),
    viteSingleFile(),
    mockDevServerPlugin({
      prefix: ["/api/config"],
    }),
  ],
});
