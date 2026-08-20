import { join } from "path";
import bindings from "node-gyp-build";

export const WM_CASEFOLD = 1;
export const WM_PATHNAME = 2;

export const WM_MATCH = 0;
export const WM_NOMATCH = 1;

export interface WildmatchFlags {
  casefold?: boolean;
  pathname?: boolean;
}

interface WildmatchAddon {
  wildmatch(pattern: string, text: string, flags: number): boolean;
  wildmatchPos(pattern: string, text: string, flags: number): number;
  wildmatchMany(patterns: string[], texts: string[], flags: number): Uint32Array;
}

const addon = bindings<WildmatchAddon>(join(__dirname, ".."));

function flagsToNumber(flags?: WildmatchFlags): number {
  if (!flags) return 0;
  let result = 0;
  if (flags.casefold) result |= WM_CASEFOLD;
  if (flags.pathname) result |= WM_PATHNAME;
  return result;
}

export function wildmatch(
  pattern: string,
  path: string,
  flags?: WildmatchFlags | number,
): boolean {
  const flagsNum = typeof flags === "number" ? flags : flagsToNumber(flags);
  return addon.wildmatch(pattern, path, flagsNum);
}

export function wildmatchPos(
  pattern: string,
  path: string,
  flags?: WildmatchFlags | number,
): number {
  const flagsNum = typeof flags === "number" ? flags : flagsToNumber(flags);
  return addon.wildmatchPos(pattern, path, flagsNum);
}

export function wildmatchMany(
  patterns: string[],
  paths: string[],
  flags?: WildmatchFlags | number,
): Set<string> {
  const flagsNum = typeof flags === "number" ? flags : flagsToNumber(flags);
  // The addon returns the indices of the matching paths; materialising the
  // strings again on the native side would cost two boundary crossings each.
  const indices = addon.wildmatchMany(patterns, paths, flagsNum);

  const out = new Set<string>();
  for (let i = 0; i < indices.length; i++) out.add(paths[indices[i]]);
  return out;
}
