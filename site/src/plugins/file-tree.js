import { processFileTree } from "../../node_modules/@astrojs/starlight/dist/user-components/file-tree-processor.js";

// Deep relative import: the processor lives behind Starlight's package
// `exports` map, which has no subpath for it — a plain file path keeps
// working with the pinned dependency.

const ESCAPES = { "&": "&amp;", "<": "&lt;", ">": "&gt;" };
const escapeHtml = (s) => s.replace(/[&<>]/g, (c) => ESCAPES[c]);

/**
 * Parse an indented tree listing into nested `<ul>` HTML — the input shape
 * Starlight's processFileTree() expects. Accepts both the GitHub tree style
 * (`src/` / `  main.rs — comment`) and the `- item` list style, so fences
 * stay readable as plain code blocks on GitHub.
 */
function linesToHtmlList(lines) {
  const root = { children: [] };
  const stack = [{ indent: -1, node: root }];

  for (const raw of lines) {
    const m = /^(\s*)(?:- )?(.*)$/.exec(raw);
    const text = m?.[2]?.trim();
    if (!text) continue;
    const indent = m[1].length;
    while (stack.length > 1 && stack.at(-1).indent >= indent) stack.pop();
    const item = { text, children: [] };
    stack.at(-1).node.children.push(item);
    stack.push({ indent, node: item });
  }

  const render = (items) =>
    `<ul>${items
      .map((i) => `<li>${escapeHtml(i.text)}${i.children.length ? render(i.children) : ""}</li>`)
      .join("")}</ul>`;
  return render(root.children);
}

/**
 * Sätteri hast plugin: replace a ```file-tree fence's rendered `<pre>` with
 * the Starlight file-tree markup (seti icons, collapsible dirs). Runs in the
 * hast phase because Sätteri 0.3.8 breaks pages on mdast `code` mutations;
 * hast `element` visitors with `{ type: "raw" }` replacements work. On any
 * error the fence is left as a plain code block (GitHub stays happy).
 */
function satteriFileTreePlugin() {
  return {
    name: "file-tree",
    element: {
      filter: ["pre"],
      visit(node, ctx) {
        const className = node.children?.[0]?.properties?.className;
        if (!Array.isArray(className) || !className.includes("language-file-tree")) return;
        try {
          const body = ctx.textContent(node).replace(/\n+$/, "");
          const listHtml = linesToHtmlList(body.split("\n"));
          const html = processFileTree(listHtml, "Directory");
          ctx.replaceNode(node, {
            type: "raw",
            value: `<starlight-file-tree class="not-content" data-pagefind-ignore>${html}</starlight-file-tree>`,
          });
        } catch (err) {
          console.error("[file-tree] leaving fence as code block:", err);
        }
      },
    },
  };
}

/** Starlight plugin registering the transform on the active processor. */
export function fileTree() {
  return {
    name: "file-tree",
    hooks: {
      "config:setup"({ addIntegration }) {
        addIntegration({
          name: "file-tree-processor",
          hooks: {
            "astro:config:setup": ({ config, updateConfig }) => {
              const processor = config.markdown?.processor;
              if (processor?.name === "satteri") {
                processor.options.hastPlugins.push(satteriFileTreePlugin());
                updateConfig({ markdown: { processor } });
                return;
              }
              console.warn("[file-tree] satteri processor not active — fences stay code blocks");
            },
          },
        });
      },
    },
  };
}
