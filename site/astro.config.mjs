import fs from "node:fs";
import path from "node:path";

import starlight from "@astrojs/starlight";
import catppuccin from "@catppuccin/starlight";
import { defineConfig } from "astro/config";
import { starlightBasePath } from "starlight-base-path";
import githubAlerts from "starlight-github-alerts";
import starlightHeadingBadges from "starlight-heading-badges";
import starlightImageZoom from "starlight-image-zoom";
import starlightLinksValidator from "starlight-links-validator";

import { fileTree } from "./src/plugins/file-tree.js";
import { issueTable } from "./src/plugins/issue-table.js";
import { mdLinkToRoute } from "./src/plugins/md-link-to-route.js";

// DOCS_BASE overrides the base for per-branch deploys (deploy-docs.yml sets
// it to /dmx-interface/branch/<ref>); local builds use the default.
const base = process.env.DOCS_BASE ?? "/dmx-interface";

// Site title and default meta description are single-sourced from the
// homepage frontmatter (docs/index.md — the repo-root README.md symlinks to
// it). Both are only defaults; every page overrides them with its own
// frontmatter. Restart the dev server after editing them there.
function homepageFrontmatter() {
  const md = fs.readFileSync(new URL("../docs/index.md", import.meta.url), "utf8");
  const frontmatter = /^---\n([\s\S]*?)\n---/.exec(md)?.[1];
  if (!frontmatter) throw new Error("docs/index.md: missing YAML frontmatter");
  const field = (key) => {
    const value = new RegExp(`^${key}:\\s*(.+)$`, "m").exec(frontmatter)?.[1];
    if (!value) throw new Error(`docs/index.md: missing frontmatter \`${key}\``);
    return value.trim();
  };
  return { title: field("title"), description: field("description") };
}

const home = homepageFrontmatter();

// Vite's dev server serves public/ files but not directory index.html for
// bare directory URLs (`/api/`, `/api/dmx_interface/`) — production hosts
// do resolve those. Rewrite such requests to the explicit index.html so the
// rustdoc tree under public/api/ is usable in `inv docs:dev`. Requests
// for real Astro/Starlight routes fall through untouched (their index.html
// does not exist under public/).
// Astro strips the base before Vite middlewares see the request, so paths
// arrive unprefixed (`/api/`, not `/dmx-interface/api/`).
function devPublicDirIndex() {
  return {
    name: "dev-public-dir-index",
    configureServer(server) {
      server.middlewares.use((req, _res, next) => {
        const url = new URL(req.url ?? "/", "http://localhost");
        let pathname = decodeURIComponent(url.pathname);
        if (pathname.startsWith(base)) pathname = pathname.slice(base.length);
        if (!pathname || path.extname(pathname)) return next();
        const dir = pathname.endsWith("/") ? pathname : `${pathname}/`;
        if (fs.existsSync(path.join("public", dir, "index.html"))) {
          req.url = `${dir}index.html${url.search}`;
        }
        next();
      });
    },
  };
}

export default defineConfig({
  site: "https://hendrikrauh.github.io",
  base,
  vite: {
    plugins: [devPublicDirIndex()],
    server: {
      fs: {
        // Design assets live in the repo-root assets/ directory (banner
        // symlinks and content images resolve there); Vite's default
        // workspace root is site/ and would 403 them in dev.
        allow: [path.resolve(import.meta.dirname, "..")],
      },
    },
  },
  integrations: [
    starlight({
      title: home.title,
      description: home.description,
      // Wordmark (extracted from assets/design/social.svg), shared with the
      // device UI (Catppuccin latte/mocha). Passed as "./..." path strings
      // that Starlight resolves against the site root and imports in the app
      // build. Do not ESM-import the SVGs here: the config loader is plain
      // Vite and small SVGs become a data: URI there, which Starlight's
      // virtual module would then import as code (Node cannot link
      // image/svg+xml). Starlight's schema is a union: light/dark without
      // src — src+light/dark would silently drop the light variant.
      logo: {
        light: "./src/assets/banner-latte.svg",
        dark: "./src/assets/banner-mocha.svg",
        alt: home.title,
        replacesTitle: true,
      },
      // Hero shows the wordmark above the tagline (MDX-less Starlight
      // pages cannot inline the SVG in frontmatter).
      components: {
        Hero: "./src/components/BannerHero.astro",
      },
      customCss: ["./src/styles/custom.css"],
      favicon: "/favicon.svg",
      // Content lives in the repo-root docs/ directory (see
      // src/content.config.ts). Markdown plugins that filter by path
      // (starlight-github-alerts) consult this list.
      markdown: {
        processedDirs: ["../docs"],
      },
      social: [
        {
          icon: "github",
          label: "GitHub",
          href: "https://github.com/HendrikRauh/dmx-interface",
        },
      ],
      sidebar: [
        { label: "Overview", items: ["index"] },
        {
          label: "Setup",
          items: ["getting-started", "hardware", "flashing", "case"],
        },
        {
          label: "Usage",
          items: [
            "config",
            {
              label: "Network",
              slug: "network",
              badge: { text: "SOON", variant: "note" },
            },
            {
              label: "HTTP & WebSocket API",
              slug: "http-ws",
              badge: { text: "SOON", variant: "note" },
            },
            "troubleshooting",
          ],
        },
        {
          label: "Development",
          items: ["development", "documentation", "web", "architecture", "roadmap"],
        },
      ],
      plugins: [
        // Rewrites `./page.md` links to routes; must be registered before
        // base-path so the base prefix applies to the rewritten links.
        mdLinkToRoute(),
        // ```file-tree fences → Starlight file-tree markup (seti icons).
        fileTree(),
        // ```issues fences → tables of open GitHub issues (roadmap,
        // troubleshooting); fetches at build time, warns inline when offline.
        issueTable(),
        // Keeps root-relative links working under base: '/dmx-interface'.
        starlightBasePath(),
        catppuccin({
          dark: { flavor: "mocha", accent: "teal" },
          light: { flavor: "latte", accent: "teal" },
        }),
        // Validates internal links during `astro build` (replaces the old
        // dist-scanning scripts/check-site-links.py). The two splash hero
        // links are root-relative without base (base-prefixed only at render
        // time), so the validator cannot match them against built pages.
        starlightLinksValidator({
          exclude: ["/getting-started/", "/development/"],
        }),
        // Click-to-zoom for content images (circuit diagram, screenshots).
        starlightImageZoom(),
        // `:badge[SOON]{variant=note}` syntax on under-construction headings.
        starlightHeadingBadges(),
        // Transforms `> [!NOTE]` blockquotes into Starlight asides. Must run
        // last: an earlier integration replaces the markdown processor, and
        // plugins registered before that lose their mdast handlers.
        githubAlerts(),
      ],
    }),
  ],
});
