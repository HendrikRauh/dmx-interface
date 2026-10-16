import { visit } from "unist-util-visit";

/**
 * Sätteri mdast plugin: rewrite intra-docs links (`./page.md`,
 * `./page.md#anchor`) to Starlight routes (`/page/`, `/page/#anchor`).
 * GitHub keeps rendering the `.md` form from the same source file; the site
 * needs routes. Sätteri nodes are read-only — edits go through the visitor
 * context, like starlight-base-path's mdast plugin.
 */
function satteriMdLinkToRoute() {
  return {
    name: "md-link-to-route",
    link(node, ctx) {
      const match = /^\.\/([^#]+)\.md(#.*)?$/.exec(node.url);
      if (match) {
        ctx.setProperty(node, "url", `/${match[1]}/${match[2] ?? ""}`);
      }
    },
  };
}

/**
 * Legacy remark variant of the same rewrite, for the unified processor
 * (Astro's `@astrojs/markdown-remark` path).
 */
function remarkMdLinkToRoute() {
  return (tree) => {
    visit(tree, "link", (node) => {
      const match = /^\.\/([^#]+)\.md(#.*)?$/.exec(node.url);
      if (match) {
        node.url = `/${match[1]}/${match[2] ?? ""}`;
      }
    });
  };
}

/**
 * Starlight plugin that registers the link rewrite on whichever markdown
 * processor Astro configured (satteri or unified), mirroring how
 * starlight-base-path augments the processor in place. Must be listed
 * *before* starlight-base-path so the base prefix applies to the rewritten
 * root-relative links.
 */
export function mdLinkToRoute() {
  return {
    name: "md-link-to-route",
    hooks: {
      "config:setup"({ addIntegration }) {
        addIntegration({
          name: "md-link-to-route-processor",
          hooks: {
            "astro:config:setup": ({ config, updateConfig }) => {
              const processor = config.markdown?.processor;
              const remarkPlugin = remarkMdLinkToRoute();

              if (processor?.name === "satteri") {
                processor.options.mdastPlugins.push(satteriMdLinkToRoute());
                updateConfig({ markdown: { processor } });
                return;
              }

              updateConfig({
                markdown: { remarkPlugins: [remarkPlugin] },
              });
            },
          },
        });
      },
    },
  };
}
