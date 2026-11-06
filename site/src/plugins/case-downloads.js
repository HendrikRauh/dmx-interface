import fs from "node:fs";

const ESCAPES = { "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" };
const escapeHtml = (s) => s.replace(/[&<>"]/g, (c) => ESCAPES[c]);

/**
 * Turn a ```case-downloads fence into a downloads table built from
 * assets/case/output/manifest.json (written by main_case.py), so docs/case.md
 * never hardcodes variant filenames. Links are prefixed with `base` here:
 * starlight-base-path only rewrites mdast links, not raw HTML.
 */
function caseDownloadsPlugin({ base, manifestPath }) {
  // Read once per build; an error is kept so the warning shows on every page.
  let cached;

  const loadManifest = () => {
    if (cached === undefined) {
      try {
        cached = JSON.parse(fs.readFileSync(manifestPath, "utf8"));
      } catch (err) {
        cached = err;
      }
    }
    return cached;
  };

  const prefix = base.replace(/\/$/, "");

  const variantRow = (variant) => {
    // `download` forces a save instead of navigation: hosts serve .step (and
    // friends) as text/plain, which browsers would render inline.
    const cell = (label, file) =>
      `<td><a href="${escapeHtml(`${prefix}/case/${file}`)}" download="${escapeHtml(file)}">${label}</a></td>`;
    return `<tr><td>${escapeHtml(variant.description)}</td>${cell("LID", variant.lid)}${cell(
      "BOX",
      variant.box,
    )}${cell("STEP", variant.step)}</tr>`;
  };

  const downloadsHtml = (manifest) => {
    const rows = manifest.variants.map(variantRow).join("");
    const glb = manifest.preview_glb
      ? `<p><a href="${escapeHtml(`${prefix}/case/${manifest.preview_glb}`)}" download="${escapeHtml(
          manifest.preview_glb,
        )}">GLB of the 3D preview above (default variant)</a></p>`
      : "";
    // No thead: the short link texts (LID/BOX/STEP) are the column hints.
    return `<table><tbody>${rows}</tbody></table>${glb}`;
  };

  const warningHtml = (message) => `<blockquote><p>${escapeHtml(message)}</p></blockquote>`;

  return {
    name: "case-downloads",
    element: {
      filter: ["pre"],
      visit(node, ctx) {
        const className = node.children?.[0]?.properties?.className;
        if (!Array.isArray(className) || !className.includes("language-case-downloads")) return;
        const manifest = loadManifest();
        if (manifest instanceof Error) {
          console.error(`[case-downloads] ${manifest.message}`);
          ctx.replaceNode(node, {
            type: "raw",
            value: warningHtml(
              `The case download files are not generated (${manifest.message}); run \`nix develop -c inv case:generate\`.`,
            ),
          });
          return;
        }
        try {
          ctx.replaceNode(node, { type: "raw", value: downloadsHtml(manifest) });
        } catch (err) {
          console.error("[case-downloads] leaving fence as code block:", err);
        }
      },
    },
  };
}

/** Starlight plugin registering the transform on the active processor. */
export function caseDownloads(options) {
  return {
    name: "case-downloads",
    hooks: {
      "config:setup"({ addIntegration }) {
        addIntegration({
          name: "case-downloads-processor",
          hooks: {
            "astro:config:setup": ({ config, updateConfig }) => {
              const processor = config.markdown?.processor;
              if (processor?.name === "satteri") {
                processor.options.hastPlugins.push(caseDownloadsPlugin(options));
                updateConfig({ markdown: { processor } });
                return;
              }
              console.warn(
                "[case-downloads] satteri processor not active — fences stay code blocks",
              );
            },
          },
        });
      },
    },
  };
}
