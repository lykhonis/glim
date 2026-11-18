// @ts-check

/** @param {string} text @param {string} version */
export function changelogSection(text, version) {
  const heading = `## [${version}]`;
  const lines = text.split(/\r?\n/);
  const start = lines.findIndex((line) => line === heading || line.startsWith(`${heading} `));
  if (start < 0) throw new Error(`CHANGELOG.md has no ${heading} section`);
  let end = lines.length;
  for (let i = start + 1; i < lines.length; i++) {
    if (lines[i].startsWith("## ") || /^\[[^\]]+\]: /.test(lines[i])) {
      end = i;
      break;
    }
  }
  const body = lines.slice(start + 1, end).join("\n").trim();
  if (!body) throw new Error(`CHANGELOG.md section ${heading} is empty`);
  return `${lines[start]}\n\n${body}\n`;
}

// Every section links to its tag (or, for Unreleased, the compare view).
/** @param {string} text */
export function checkChangelogLinks(text) {
  const sections = [...text.matchAll(/^## \[([^\]]+)\]/gm)].map((m) => m[1]);
  if (sections[0] !== "Unreleased") throw new Error("CHANGELOG.md must open with ## [Unreleased]");
  const links = new Set([...text.matchAll(/^\[([^\]]+)\]: https?:\/\/\S+$/gm)].map((m) => m[1]));
  const missing = sections.filter((s) => !links.has(s));
  if (missing.length) throw new Error(`CHANGELOG.md sections without a link: ${missing.join(", ")}`);
}
