import { describe, it, expect } from "vitest";
import {
  wildmatch,
  wildmatchMany,
  WM_CASEFOLD,
  WM_PATHNAME,
} from "../src/index";

// wildmatchMany compiles patterns into fast paths (literal / "*"+tail) that
// bypass dowild(). Those fast paths must be *exactly* equivalent to running
// wildmatch() on every pair, for every flag combination. This test is the
// oracle that keeps them honest.

function rng(seed: number) {
  let s = seed >>> 0 || 1;
  return () => {
    s ^= s << 13; s >>>= 0;
    s ^= s >>> 17;
    s ^= s << 5; s >>>= 0;
    return s / 4294967296;
  };
}

const PATTERN_ATOMS = [
  "*", "**", "?", "[a-z]", "[!a-z]", "[[:digit:]]", "a", "B", "foo", ".ts",
  ".TS", "/", "src", "*.ts", "\\*", "-", "]", "[", "z9",
];
const TEXT_ATOMS = [
  "a", "B", "foo", ".ts", ".TS", "/", "src", "z9", "-", "]", "[", "*", "0",
  "Foo/Bar", "..", "\u0000",
];

function build(r: () => number, atoms: string[], maxParts: number) {
  const n = 1 + Math.floor(r() * maxParts);
  let out = "";
  for (let i = 0; i < n; i++) out += atoms[Math.floor(r() * atoms.length)];
  return out;
}

const FLAG_SETS = [0, WM_PATHNAME, WM_CASEFOLD, WM_PATHNAME | WM_CASEFOLD];

describe("wildmatchMany differential vs wildmatch", () => {
  for (const flags of FLAG_SETS) {
    it(`agrees pair-by-pair for flags=${flags}`, () => {
      const r = rng(0xc0ffee + flags);
      for (let round = 0; round < 200; round++) {
        const patterns = Array.from({ length: 6 }, () => build(r, PATTERN_ATOMS, 4));
        const texts = Array.from({ length: 10 }, () => build(r, TEXT_ATOMS, 4));

        const expected = new Set(
          texts.filter((t) => patterns.some((p) => wildmatch(p, t, flags))),
        );
        expect(wildmatchMany(patterns, texts, flags), `${JSON.stringify({ patterns, texts, flags })}`)
          .toEqual(expected);
      }
    });
  }

  // wildmatchMany only builds its bucket indexes above a pattern-count threshold
  // (currently 24 per class), so a small fuzz round would silently skip the
  // indexed code path entirely. These rounds stay well above it.
  for (const flags of FLAG_SETS) {
    it(`agrees pair-by-pair with the bucket indexes active, flags=${flags}`, () => {
      const r = rng(0x5eed + flags);
      for (let round = 0; round < 60; round++) {
        const patterns = Array.from({ length: 80 }, () => build(r, PATTERN_ATOMS, 4));
        const texts = Array.from({ length: 25 }, () => build(r, TEXT_ATOMS, 4));
        const expected = new Set(
          texts.filter((t) => patterns.some((p) => wildmatch(p, t, flags))),
        );
        expect(wildmatchMany(patterns, texts, flags), `${JSON.stringify({ patterns, texts, flags })}`)
          .toEqual(expected);
      }
    });
  }

  it("handles strings far beyond the arena's per-element size guess", () => {
    // fill_arena() speculates ~48 bytes per element and grows + retries when a
    // copy might have been truncated. Exercise that path in both arrays.
    const long = "a".repeat(5000);
    const paths = [`${long}.ts`, `${"b".repeat(200)}.js`, "x.ts", ""];
    expect(wildmatchMany(["*.ts"], paths, 0)).toEqual(new Set([`${long}.ts`, "x.ts"]));
    expect(wildmatchMany([`${long}.ts`], paths, 0)).toEqual(new Set([`${long}.ts`]));
    // A long run of growing strings, so growth happens mid-array many times.
    const growing = Array.from({ length: 300 }, (_, i) => `${"d".repeat(i * 7)}/f.ts`);
    expect(wildmatchMany(["**/*.ts"], growing, WM_PATHNAME).size).toBe(growing.length);
    expect(wildmatchMany(growing, growing, 0).size).toBe(growing.length);
  });

  it("stays consistent with wildmatch when a string contains a NUL", () => {
    // dowild() is C-string based and stops at the first NUL, and the length-based
    // fast paths must agree with it rather than matching past the NUL.
    // NOTE: the shared truncation itself is a known bug -- "we\0ird.ts" is matched
    // as "we" -- so this test pins consistency, not correctness.
    const paths = ["a.ts", "we\u0000ird.ts", "b.js", "\u0000", "x\u0000y"];
    expect(wildmatchMany(["*.ts"], paths, 0)).toEqual(
      new Set(paths.filter((p) => wildmatch("*.ts", p, 0))),
    );
    expect(wildmatchMany(["*\u0000*"], paths, 0)).toEqual(
      new Set(paths.filter((p) => wildmatch("*\u0000*", p, 0))),
    );
    // NUL in the patterns only.
    expect(wildmatchMany(["a\u0000b", "*.js"], paths, 0)).toEqual(new Set(["b.js"]));
    // Empty strings are not NULs and must stay on the fast path.
    expect(wildmatchMany(["", "*.ts"], ["", "a.ts"], 0)).toEqual(new Set(["", "a.ts"]));
  });

});
