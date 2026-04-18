"use strict";

// Benchmark wildmatch against a synthetic .gitattributes file with 10k
// pattern entries, simulating how git evaluates attribute patterns: for a
// given path, walk every pattern and run wildmatch with WM_PATHNAME.
//
// Run:
//   node bench/wildmatch.bench.js
//   node --allow-natives-syntax bench/wildmatch.bench.js   # better accuracy

import path from "node:path";
import { fileURLToPath } from "node:url";
import { Suite, chartReport } from "bench-node";

const __dirname = path.dirname(fileURLToPath(import.meta.url));
import { wildmatch, wildmatchMany, WM_PATHNAME, WM_CASEFOLD } from "../dist/index.mjs";
import { ensureFile, parsePatterns } from "./generate-gitattributes.mjs";

function formatNs(ns) {
  if (!Number.isFinite(ns)) return "n/a";
  if (ns >= 1e9) return `${(ns / 1e9).toFixed(2)}s`;
  if (ns >= 1e6) return `${(ns / 1e6).toFixed(2)}ms`;
  if (ns >= 1e3) return `${(ns / 1e3).toFixed(2)}us`;
  return `${ns.toFixed(2)}ns`;
}

// sampleData is pre-sorted ascending (bench-node's StatisticalHistogram sorts
// during outlier removal). Index formula matches the library's own
// percentile(); we additionally tolerate empty input and out-of-range p.
function percentile(sortedSamples, p) {
  const n = sortedSamples.length;
  if (n === 0) return NaN;
  if (p <= 0) return sortedSamples[0];
  if (p >= 100) return sortedSamples[n - 1];
  return sortedSamples[Math.ceil(n * (p / 100)) - 1];
}

// Custom reporter: the default chart + a latency-percentile table that
// includes p50/p75/p95/p99/p99.9 computed from each benchmark's histogram.
function chartAndPercentileReport(results, options) {
  chartReport(results, options);

  const nameWidth = Math.max(9, ...results.map((r) => r.name.length));
  const colWidth = 10;

  // Single ordered column spec so header labels and row values can't drift.
  const columns = [
    ["min", (h, _s) => h.min],
    ["p50", (h, s) => percentile(s, 50)],
    ["p75", (h, s) => percentile(s, 75)],
    ["p95", (h, s) => percentile(s, 95)],
    ["p99", (h, s) => percentile(s, 99)],
    ["p99.9", (h, s) => percentile(s, 99.9)],
    ["max", (h, _s) => h.max],
    // result.histogram omits `mean`, so compute it from sampleData.
    ["mean", (h, s) => (s.length ? s.reduce((a, b) => a + b, 0) / s.length : NaN)],
  ];

  const header =
    "Benchmark".padEnd(nameWidth) +
    "  " +
    columns.map(([label]) => label.padStart(colWidth)).join(" ");
  const rule = "-".repeat(header.length);

  process.stdout.write("\nLatency per operation (lower is better):\n");
  process.stdout.write(`${header}\n${rule}\n`);

  for (const r of results) {
    const h = r.histogram;
    const samples = h.sampleData;
    const row =
      r.name.padEnd(nameWidth) +
      "  " +
      columns.map(([, fn]) => formatNs(fn(h, samples)).padStart(colWidth)).join(" ");
    process.stdout.write(`${row}\n`);
  }
}

const ENTRY_COUNT = Number(process.env.BENCH_ENTRIES || 10_000);
const FILE = path.join(__dirname, `.gitattributes-${ENTRY_COUNT}`);

const content = ensureFile(ENTRY_COUNT, FILE);
const patterns = parsePatterns(content);

if (patterns.length !== ENTRY_COUNT) {
  throw new Error(`Expected ${ENTRY_COUNT} patterns, parsed ${patterns.length}`);
}

// A representative set of paths covering early-match, late-match and no-match
// scenarios so we don't accidentally measure only one extreme.
const PATHS = {
  earlyExtMatch: "foo.ts",
  midDirMatch: "src/index.ts",
  deepNestedMatch: "src/a/b/c/d/e/file.tsx",
  noMatch: "totally/unrelated/path/to/some-binary-blob.qzx",
  longPath: "pkg/internal/sub0042/another/very/deeply/nested/module/file00123.ts",
};

function scanAll(p, flags) {
  // Mimic git's "scan every pattern" behaviour. Last match wins, but we only
  // care about the matching cost here — we still touch every pattern.
  let lastMatchIndex = -1;
  for (let i = 0; i < patterns.length; i++) {
    if (wildmatch(patterns[i], p, flags)) {
      lastMatchIndex = i;
    }
  }
  return lastMatchIndex;
}

function firstMatch(p, flags) {
  // Short-circuit on first match — closer to "does any rule apply?" workloads.
  for (let i = 0; i < patterns.length; i++) {
    if (wildmatch(patterns[i], p, flags)) return i;
  }
  return -1;
}

// minSamples=100 ensures p99/p99.9 have enough data points to be meaningful
// (default is 10, which makes p99 essentially equal to max).
const suite = new Suite({
  reporter: chartAndPercentileReport,
  minSamples: Number(process.env.BENCH_MIN_SAMPLES || 100),
});

for (const [label, p] of Object.entries(PATHS)) {
  suite.add(`scan-all  WM_PATHNAME            ${label}`, () => {
    scanAll(p, WM_PATHNAME);
  });
}

for (const [label, p] of Object.entries(PATHS)) {
  suite.add(`first-match WM_PATHNAME          ${label}`, () => {
    firstMatch(p, WM_PATHNAME);
  });
}

suite.add("scan-all  WM_PATHNAME|CASEFOLD    midDirMatch", () => {
  scanAll(PATHS.midDirMatch, WM_PATHNAME | WM_CASEFOLD);
});

suite.add("single wildmatch call (baseline)", () => {
  wildmatch("src/**/*.ts", "src/a/b/c/d/file.ts", WM_PATHNAME);
});

// Batch: scan all 10k patterns against all 10 paths in one native call
const ALL_PATHS = Object.values(PATHS);
suite.add("batch-scan 10k×10paths  WM_PATHNAME", () => {
  const matched = wildmatchMany(patterns, ALL_PATHS, WM_PATHNAME);
  // matched is now Set<string> of texts that matched at least one pattern
});

console.log(`Loaded ${patterns.length} patterns from ${path.relative(process.cwd(), FILE)}`);

suite.run().catch((err) => {
  console.error(err);
  process.exit(1);
});
