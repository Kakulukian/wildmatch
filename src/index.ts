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
}

const addon = bindings<WildmatchAddon>(join(__dirname, ".."));

function flagsToNumber(flags?: WildmatchFlags): number {
  if (!flags) return 0;
  let result = 0;
  if (flags.casefold) result |= WM_CASEFOLD;
  if (flags.pathname) result |= WM_PATHNAME;
  return result;
}

export function wildmatch(pattern: string, text: string, flags?: WildmatchFlags | number): boolean {
  const flagsNum = typeof flags === "number" ? flags : flagsToNumber(flags);
  return addon.wildmatch(pattern, text, flagsNum);
}

export function wildmatchPos(
  pattern: string,
  text: string,
  flags?: WildmatchFlags | number,
): number {
  const flagsNum = typeof flags === "number" ? flags : flagsToNumber(flags);
  return addon.wildmatchPos(pattern, text, flagsNum);
}
