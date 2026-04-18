# git-wildmatch

`git-wildmatch` wraps git's `wildmatch.c` as a native Node binding. Same matching behavior as `git check-ignore`.

Most JS glob libraries diverge from git on edge cases: `**` placement, character classes, bracket escaping, trailing slashes. Rather than reverse-engineer git's behavior, just call the C code directly.

## Install

```sh
pnpm install git-wildmatch
```

## Usage

```ts
import { wildmatch } from "git-wildmatch";

wildmatch("*.ts", "foo.ts"); // true
wildmatch("src/**/*.ts", "src/a/b/c.ts", { pathname: true }); // true
wildmatch("README.md", "readme.md", { casefold: true }); // true
```

## API

### `wildmatch(pattern, text, flags?) → boolean`

- `pattern` — git-style pattern
- `text` — path or string to test
- `flags` — either `{ pathname?, casefold? }` or an OR'd bitfield of `WM_PATHNAME` / `WM_CASEFOLD`

### `wildmatchPos(pattern, text, flags?) → number`

Returns `0` for match, `1` for no match.

## Flags

| Flag          | Bit | Effect                                                                    |
| ------------- | --- | ------------------------------------------------------------------------- |
| `WM_PATHNAME` | 2   | `*` doesn't cross `/`; `**` between slashes matches zero or more segments |
| `WM_CASEFOLD` | 1   | Case-insensitive matching                                                 |

## Why not minimatch / micromatch?

These libraries target shell-glob semantics, not git's. The differences bite you around `**` placement, POSIX character classes, bracket escaping, and trailing slashes. `git-wildmatch` ships the actual upstream code, so it matches git exactly like `git check-ignore` and `git check-attr`

## License

The vendored `wildmatch.c` is GPL-2.0 (from git). The binding code is MIT.
