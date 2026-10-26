# Contributing to libmad

Thank you for your interest in contributing to libmad! Before you get started,
we have a few things here that you should read below. If you are ever in doubt
about creating a PR, feel free to discuss it in an issue, in one of
Tenacity's Matrix rooms, or on Tenacity's developer mailing list.

# Coding Standards

New code introduced to libmad will be written in C99. PRs migrating any
existing code away from prior standards, provided they don't break the ABI or
API, will be welcomed.

## Introducing Dependencies

libmad's only dependency is `libc`. That's it. Please keep it that way.

You may use any OS-specific system calls if needed, but we prefer that you use
any `libc` equivalents if possible. Don't forget to `#ifdef` your code behind
the proper platform detection macro too! :)

## Formatting

Please use 2 levels of indentation for both existing and new source files.
Indents should also use **spaces**, not tabs.

Do **NOT** introduce DOS line endings (i.e., `\r\n` endings) into existing
source files; do not introduce new source files with these line endings. You
can use `dos2unix` to convert these line endings to the Unix (i.e., `\n`)
format.

## Adding New Files

Please see Formatting above to ensure you format your new source files
correctly.

Only public headers should go in `include/`. Do not all private headers; please
keep them in `src/`, like with all other source files.

Examples demonstrating the libary's functionality should go in the `examples/`
folder.

All other source files should go in `src/`.

## Breaking the API/ABI

Unless necessary, **DO NOT BREAK THE API OR ABI UNLESS THERE WAS PRIOR
AGREEMENT**. Both the current API and ABI are compatible with applications
built against 0.15.1b. Many applications still use (a patched version of)
0.15.1b. Because this fork aims to replace 0.15.1b, compatibility with this
version **MUST** be preserved.

At one point, we may break the API and/or ABI, **but only if it's necessary**.
The only time we may break both is between new major versions (e.g., 0.17,
0.18, 1.0, etc). Even then, we try not to break either unnecessarily, so please
preserve the API or ABI wherever possible.

