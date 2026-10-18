const DEFAULT_REPO = "HendrikRauh/dmx-interface";

// One entry per label per build; a fetch error is cached too so a rate-limit
// warn appears once, not once per page.
const issueCache = new Map();

const ESCAPES = { "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" };
const escapeHtml = (s) => s.replace(/[&<>"]/g, (c) => ESCAPES[c]);

/**
 * Fetch open issues (PRs excluded) for a label from the GitHub REST API.
 * Authenticated via GITHUB_TOKEN/GH_TOKEN when set (deploy-docs.yml passes
 * the workflow token); unauthenticated builds are rate-limited but work.
 */
async function fetchOpenIssues(label) {
  if (issueCache.has(label)) return issueCache.get(label);
  const repo = process.env.GITHUB_REPOSITORY ?? DEFAULT_REPO;
  const token = process.env.GITHUB_TOKEN ?? process.env.GH_TOKEN;
  const url = `https://api.github.com/repos/${repo}/issues?labels=${encodeURIComponent(label)}&state=open&per_page=100&sort=updated&direction=desc`;
  try {
    const headers = {
      Accept: "application/vnd.github+json",
      "User-Agent": `${repo}-docs`,
    };
    if (token) headers.Authorization = `Bearer ${token}`;
    const res = await fetch(url, { headers });
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    const issues = (await res.json()).filter((issue) => !issue.pull_request);
    issueCache.set(label, issues);
    return issues;
  } catch (err) {
    console.warn(`[issue-table] cannot fetch issues for label '${label}': ${err.message}`);
    issueCache.set(label, err);
    return err;
  }
}

const OPTION_KEYS = new Set(["label", "exclude", "state"]);

/**
 * Read `key=value` lines from the fence body; any other line is ignored.
 * `label` is required, `state=true` adds the State column (roadmap shows
 * in-progress issues), `exclude=a,b` drops issues carrying those labels.
 */
function parseOptions(body) {
  const opts = {};
  for (const line of body.split("\n")) {
    const match = /^([a-z-]+)=(\S+)$/.exec(line.trim());
    if (!match) continue;
    const [, key, value] = match;
    if (!OPTION_KEYS.has(key))
      throw new Error(`unknown option \`${key}\` (expected ${[...OPTION_KEYS].join(", ")})`);
    opts[key] = value;
  }
  if (!opts.label) throw new Error("first line must be `label=<github-label>`");
  return opts;
}

const labelName = (label) => (typeof label === "string" ? label : label.name);

function issueTableHtml(opts, issues) {
  const showState = opts.state === "true";
  const excluded = new Set((opts.exclude ?? "").split(",").filter(Boolean));
  const rows = issues
    .filter((issue) => !issue.labels.some((l) => excluded.has(labelName(l))))
    .map((issue) => {
      const inProgress = issue.labels.some((l) => labelName(l) === "in-progress");
      const cells = [
        `<td>${escapeHtml(issue.title)}</td>`,
        `<td><a href="${escapeHtml(issue.html_url)}">#${issue.number}</a></td>`,
      ];
      if (showState) cells.push(`<td>${inProgress ? "in progress" : "open"}</td>`);
      return `<tr>${cells.join("")}</tr>`;
    })
    .join("");
  const heading = opts.label === "roadmap" ? "Feature" : "Bug";
  const header = showState
    ? `<th>${heading}</th><th>Issue</th><th>State</th>`
    : `<th>${heading}</th><th>Issue</th>`;
  return `<table><thead><tr>${header}</tr></thead><tbody>${rows}</tbody></table>`;
}

function warningHtml(message) {
  return `<blockquote><p>${escapeHtml(message)}</p></blockquote>`;
}

function issueTablePlugin() {
  return {
    name: "issue-table",
    element: {
      filter: ["pre"],
      async visit(node, ctx) {
        const className = node.children?.[0]?.properties?.className;
        if (!Array.isArray(className) || !className.includes("language-issues")) return;
        try {
          const body = ctx.textContent(node).replace(/\n+$/, "");
          const opts = parseOptions(body);
          const issues = await fetchOpenIssues(opts.label);
          if (issues instanceof Error) {
            ctx.replaceNode(node, {
              type: "raw",
              value: warningHtml(
                `GitHub issues could not be loaded (${issues.message}); check the issues on GitHub directly.`,
              ),
            });
            return;
          }
          ctx.replaceNode(node, { type: "raw", value: issueTableHtml(opts, issues) });
        } catch (err) {
          console.error("[issue-table] leaving fence as code block:", err);
        }
      },
    },
  };
}

/** Starlight plugin registering the transform on the active processor. */
export function issueTable() {
  return {
    name: "issue-table",
    hooks: {
      "config:setup"({ addIntegration }) {
        addIntegration({
          name: "issue-table-processor",
          hooks: {
            "astro:config:setup": ({ config, updateConfig }) => {
              const processor = config.markdown?.processor;
              if (processor?.name === "satteri") {
                processor.options.hastPlugins.push(issueTablePlugin());
                updateConfig({ markdown: { processor } });
                return;
              }
              console.warn("[issue-table] satteri processor not active — fences stay code blocks");
            },
          },
        });
      },
    },
  };
}
