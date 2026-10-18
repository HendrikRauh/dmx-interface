import { fileURLToPath } from "node:url";

import { docsSchema } from "@astrojs/starlight/schema";
import { glob } from "astro/loaders";
import { z } from "astro/zod";
import { defineCollection } from "astro:content";

// Canonical prose lives in the repo-root docs/ directory — AGENTS.md routes
// to the same files, and GitHub renders them natively. The repo-root
// README.md is a symlink to docs/index.md, so the GitHub landing page and
// the site homepage ("/") share one file. Starlight's built-in docsLoader is
// hardwired to src/content/docs, so load the collection from here instead.
// Relative image links (../assets/…) resolve against the real file path and
// are processed by Astro's asset pipeline.
const docsBase = fileURLToPath(new URL("../../docs", import.meta.url));

const docs = defineCollection({
  loader: glob({ pattern: "**/[^_]*.md", base: docsBase }),
  // starlight-links-validator keys its per-page data by frontmatter `slug`
  // when present (its path-derived id assumes src/content/docs, which does
  // not hold for this repo-root docs/ layout). Every page that other pages
  // link to must therefore carry `slug:` matching its route, or link
  // validation fails; index.md is exempt (it is never a link target).
  schema: docsSchema({ extend: z.object({ slug: z.string().optional() }) }),
});

// English-only site: no i18n collection (Starlight does not require one).
export const collections = { docs };
