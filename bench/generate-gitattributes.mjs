// Generate a synthetic .gitattributes-style file with N pattern entries.
// Each line is: "<pattern> <attr>=<value>" — only the first token is matched
// against paths, mirroring how git iterates attribute patterns.

import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const EXTS = [
  "ts",
  "tsx",
  "js",
  "jsx",
  "mjs",
  "cjs",
  "json",
  "md",
  "mdx",
  "yml",
  "yaml",
  "toml",
  "css",
  "scss",
  "less",
  "html",
  "htm",
  "svg",
  "png",
  "jpg",
  "jpeg",
  "gif",
  "webp",
  "ico",
  "pdf",
  "zip",
  "gz",
  "tar",
  "rs",
  "go",
  "py",
  "rb",
  "java",
  "kt",
  "swift",
  "cpp",
  "cc",
  "c",
  "h",
  "hpp",
  "lock",
  "log",
  "txt",
  "sh",
  "bash",
  "zsh",
  "fish",
  "ps1",
  "sql",
  "csv",
  "tsv",
  "xml",
  "proto",
];

const DIRS = [
  "src",
  "lib",
  "test",
  "tests",
  "spec",
  "app",
  "pkg",
  "internal",
  "cmd",
  "bin",
  "scripts",
  "tools",
  "docs",
  "examples",
  "vendor",
  "third_party",
  "assets",
  "public",
  "static",
  "config",
  "configs",
  "build",
  "dist",
  "node_modules",
  "target",
  "out",
  "tmp",
  "fixtures",
  "mocks",
];

const ATTRS = [
  "text",
  "binary",
  "text eol=lf",
  "text eol=crlf",
  "-text",
  "text=auto",
  "linguist-generated=true",
  "linguist-vendored=true",
  "diff=cpp",
  "diff=rust",
  "merge=ours",
  "export-ignore",
  "filter=lfs diff=lfs merge=lfs -text",
];

function pad(n, width) {
  const s = String(n);
  return s.length >= width ? s : "0".repeat(width - s.length) + s;
}

function generate(count) {
  const lines = [];
  const seen = new Set();

  const push = (pattern, attr) => {
    const key = pattern;
    if (seen.has(key)) return;
    seen.add(key);
    lines.push(`${pattern} ${attr}`);
  };

  // Distribution: ~60% extension globs, 20% dir globs, 10% deep globstars,
  // 5% literal files, 5% character-class patterns.
  let i = 0;
  while (lines.length < count) {
    const ext = EXTS[i % EXTS.length];
    const dir = DIRS[i % DIRS.length];
    const attr = ATTRS[i % ATTRS.length];
    const variant = i % 20;

    if (variant < 12) {
      push(`*.${ext}${i % 4 === 0 ? pad(i, 4) : ""}`, attr);
    } else if (variant < 16) {
      push(`${dir}/*.${ext}`, attr);
    } else if (variant < 18) {
      push(`${dir}/**/*.${ext}`, attr);
    } else if (variant === 18) {
      push(`${dir}/sub${pad(i, 4)}/file${pad(i, 5)}.${ext}`, attr);
    } else {
      push(`${dir}/[a-z]*${pad(i, 3)}.${ext}`, attr);
    }
    i++;
    if (i > count * 10) break; // safety
  }

  return lines.slice(0, count).join("\n") + "\n";
}

function writeFile(count, outPath) {
  const content = generate(count);
  fs.mkdirSync(path.dirname(outPath), { recursive: true });
  fs.writeFileSync(outPath, content);
  return content;
}

function ensureFile(count, outPath) {
  if (fs.existsSync(outPath)) {
    const existing = fs.readFileSync(outPath, "utf8");
    const lineCount = existing.split("\n").filter(Boolean).length;
    if (lineCount === count) return existing;
  }
  return writeFile(count, outPath);
}

function parsePatterns(content) {
  const patterns = [];
  for (const rawLine of content.split("\n")) {
    const line = rawLine.trim();
    if (!line || line.startsWith("#")) continue;
    const space = line.indexOf(" ");
    patterns.push(space === -1 ? line : line.slice(0, space));
  }
  return patterns;
}

export { generate, writeFile, ensureFile, parsePatterns };

if (process.argv[1] === __filename) {
  const count = Number(process.argv[2] || 10000);
  const out = process.argv[3] || path.join(__dirname, ".gitattributes-10k");
  writeFile(count, out);
  console.log(`Wrote ${count} entries to ${out}`);
}
