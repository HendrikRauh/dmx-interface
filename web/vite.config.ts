import fs from "fs";
import path from "path";

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
    {
      name: "inline-favicon",
      transformIndexHtml(html) {
        const generateFavicon = (file: string, colorscheme?: string) => {
          const faviconPath = path.resolve(__dirname, file);
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
            generateFavicon("src/assets/favicon-mocha.svg"),
            generateFavicon("src/assets/favicon-latte.svg", "light"),
            generateFavicon("src/assets/favicon-mocha.svg", "dark"),
          ],
        };
      },
    },
  ],
});
