import fs from "fs";
import path from "path";

import preact from "@preact/preset-vite";
import { defineConfig } from "vite";
import viteCompression from "vite-plugin-compression";
import { mockDevServerPlugin } from "vite-plugin-mock-dev-server";
import { viteSingleFile } from "vite-plugin-singlefile";

// https://vitejs.dev/config/
export default defineConfig({
  server: {
    // Node ≥17 resolves "localhost" to ::1 first, so Vite's default bind
    // ([::1]:5173) is unreachable via 127.0.0.1 — which is what Docker/Podman
    // port publishing and the devcontainer's forwardPorts connect to. Listen on
    // all addresses instead; harmless on a host, required inside a container.
    host: true,
    fs: {
      // The canonical design assets live in the repo-root assets/ directory
      // and are imported from ../../assets — Vite's default workspace root is
      // this package directory, which would 403 them in dev.
      allow: [path.resolve(import.meta.dirname, "..")],
    },
  },
  plugins: [
    preact(),
    viteSingleFile(),
    mockDevServerPlugin({
      prefix: ["/api/config"],
    }),
    {
      name: "inline-favicon",
      transformIndexHtml(html) {
        const generateFavicon = (file: string, colorscheme?: string) => {
          const faviconPath = path.resolve(import.meta.dirname, file);
          const base64 = fs.readFileSync(faviconPath).toString("base64");
          const mimeType = "image/svg+xml";
          const dataUri = `data:${mimeType};base64,${base64}`;

          return {
            tag: "link",
            attrs: {
              rel: "icon",
              type: mimeType,
              href: dataUri,
              ...(colorscheme && { media: `(prefers-color-scheme: ${colorscheme})` }),
            },
            injectTo: "head" as const,
          };
        };

        return {
          html,
          tags: [
            generateFavicon("../assets/design/favicon-mocha.svg"),
            generateFavicon("../assets/design/favicon-latte.svg", "light"),
            generateFavicon("../assets/design/favicon-mocha.svg", "dark"),
          ],
        };
      },
    },
    viteCompression(),
  ],
});
