# deps/mongoose

Vendored [Mongoose](https://github.com/cesanta/mongoose) embedded networking library.

- **Version:** 7.22 (pinned)
- **Files:** `mongoose.c`, `mongoose.h` (single-file amalgamation)
- **License:** dual GPLv2 **or** commercial (<https://mongoose.ws/licensing/>).
  See the license note in the project README — this constrains the project's own license.

To update: replace both files from the desired release tag and bump the version above,
e.g. `curl -sL -o mongoose.c https://raw.githubusercontent.com/cesanta/mongoose/<tag>/mongoose.c`.
