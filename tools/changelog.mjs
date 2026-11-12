// @ts-check

/** @param {string} text @param {string} version */
export function changelogSection(text, version) {
  const heading = `## [${version}]`;
  const lines = text.split(/\r?\n/);
  const start = lines.findIndex((line) => line === heading || line.startsWith(`${heading} `));
  if (start < 0) throw new Error(`CHANGELOG.md has no ${heading} section`);
  let end = lines.length;
  for (let i = start + 1; i < lines.length; i++) {
    if (lines[i].startsWith("## ")) {
      end = i;
      break;
    }
  }
  const body = lines.slice(start + 1, end).join("\n").trim();
  if (!body) throw new Error(`CHANGELOG.md section ${heading} is empty`);
  return `${lines[start]}\n\n${body}\n`;
}
