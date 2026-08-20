import { describe, it, expect } from "vitest";
import {
  wildmatchMany,
  wildmatchPos,
  WM_CASEFOLD,
  WM_MATCH,
  WM_NOMATCH,
  WM_PATHNAME,
} from "../src/index";

describe("wildmatchPos", () => {
  it("returns WM_MATCH on a match", () => {
    expect(wildmatchPos("foo*", "foobar")).toBe(WM_MATCH);
  });

  it("returns WM_NOMATCH on a plain mismatch", () => {
    expect(wildmatchPos("foo", "bar")).toBe(WM_NOMATCH);
  });

  it("never leaks the internal WM_ABORT_* codes", () => {
    // These patterns/texts drive dowild() into WM_ABORT_ALL (-1) and
    // WM_ABORT_TO_STARSTAR (-2) respectively.
    expect(wildmatchPos("foo*bar", "foo", WM_PATHNAME)).toBe(WM_NOMATCH);
    expect(wildmatchPos("*", "foo/bar", WM_PATHNAME)).toBe(WM_NOMATCH);
    expect(wildmatchPos("a[", "a[")).toBe(WM_NOMATCH);
    expect(wildmatchPos("a\\", "a\\")).toBe(WM_NOMATCH);
  });
});

describe("wildmatchMany", () => {
  const paths = ["src/a.ts", "src/b.js", "docs/c.md", "README.md"];

  it("returns paths matching any pattern", () => {
    expect(wildmatchMany(["src/*.ts", "*.md"], paths, WM_PATHNAME)).toEqual(
      new Set(["src/a.ts", "README.md"]),
    );
  });

  it("returns an empty set when nothing matches", () => {
    expect(wildmatchMany(["*.py"], paths, WM_PATHNAME)).toEqual(new Set());
  });

  it("honours flags", () => {
    expect(wildmatchMany(["SRC/*.TS"], paths, WM_PATHNAME)).toEqual(new Set());
    expect(wildmatchMany(["SRC/*.TS"], paths, WM_PATHNAME | WM_CASEFOLD)).toEqual(
      new Set(["src/a.ts"]),
    );
  });

  it("dedupes a path matched by several patterns", () => {
    expect(wildmatchMany(["src/*", "*.ts", "**"], paths, 0).size).toBe(paths.length);
  });

  it("rejects non-string elements", () => {
    expect(() => wildmatchMany([1 as never], paths, 0)).toThrow(TypeError);
    expect(() => wildmatchMany(["*"], [1 as never], 0)).toThrow(TypeError);
  });
});
