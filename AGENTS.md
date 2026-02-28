# AGENTS.md

## Cursor Cloud specific instructions

### Project Overview

`mod_ringback` is a FreeSWITCH C module for ringback/busy/congestion tone detection via Goertzel algorithm on 450Hz. Pure C, no external runtime dependencies.

### Build & Test

- **Unit tests** (primary development validation): `make test` — compiles and runs `test/tone_detect_test.c`. Requires only `gcc` and `libm`. No FreeSWITCH dependency.
- **Module build** (`mod_ringback.so`): requires FreeSWITCH source headers. Clone them with `git clone --depth 1 https://github.com/signalwire/freeswitch.git /tmp/freeswitch`, then generate a minimal `switch_am_config.h` stub before compiling. Run `make FS_SRC=/tmp/freeswitch`.
- **Lint**: GCC warnings serve as the linter (`-Wall -Wextra` flags in `test/Makefile`). No separate linting tool is configured.
- There is no separate "dev server" to run — this is a compiled shared library loaded by FreeSWITCH at runtime.

### Key Gotchas

- The module source (`src/mod_ringback.c`) may have API compatibility differences with the latest FreeSWITCH master branch. The CI workflow uses `continue-on-error: true` for the full FreeSWITCH tree build. Unit tests are always the reliable development check.
- `autoconf`, `automake`, and `libtool` are needed if you want to run FreeSWITCH's `bootstrap.sh` + `configure` to generate proper headers. For standalone module compilation, a minimal `switch_am_config.h` stub is sufficient to get past the first header hurdle.
