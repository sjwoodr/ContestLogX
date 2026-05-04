# Contributing to ContestLogX

Thanks for your interest in ContestLogX. This document covers what you need to know before opening a pull request.

## Ground Rules

- ContestLogX is cross-platform: every change must work on **Linux, macOS, and Windows**. Use `Q_OS_LINUX` / `Q_OS_MACOS` / `Q_OS_WIN` guards for platform-specific code, and avoid GCC/Clang-only extensions (the Windows build uses MSVC).
- Use `QDir`, `QStandardPaths`, and `/` separators for paths — Qt normalizes them.
- C++17 only. No new third-party dependencies without discussion in an issue first.
- Keep changes focused. One logical change per PR. Refactors that touch unrelated code are easier to review as separate PRs.

## Getting Set Up

### Linux (Ubuntu/Debian)
```bash
sudo apt install cmake build-essential qt6-base-dev libqt6serialport6-dev libqt6xml6-dev qt6-multimedia-dev
```

### macOS
```bash
brew install qt6 cmake
export CMAKE_PREFIX_PATH=$(brew --prefix qt6)
```

### Build & Run
```bash
make              # build
./clx             # run
./clx --debug --log <logfile.clx>  # debug mode
```

See [`docs/BUILD.md`](docs/BUILD.md) for Windows build instructions and full details.

## Testing

**Both test suites must pass before submitting a PR:**

```bash
make test                # unit tests
make test-logs-headless  # contest log validation (parallel — faster than make test-logs)
```

`make test-logs` validates scores AND multiplier details for every supported contest using the logs in `test_logs/`. If you change the contest engine, scoring, or any contest definition, this catches regressions across the rest of the contests.

If you add a new contest, add a corresponding entry in `test_logs/automated_tests.json` with an expected score and multiplier breakdown.

## Code Conventions

### Naming
- **Exchange fields:** 3-letter codes with `s` (sent) or `r` (received) suffix — `RSTs`, `RSTr`, `SNs`, `SNr`, `EXCHs`, `EXCHr`, `GRIDs`, `GRIDr`, `NAMEs`, `NAMEr`
- **QSO log columns:** uppercase — `DATE`, `TIME`, `CALL`, `FREQ`, `MODE`, `POINTS`
- **Mode tracking:** the score widget collapses SSB/AM/FM into `PH` (PHONE)

### Adding a New Contest
Most contests can be added by dropping a JSON file into `contests/` — no C++ changes required. See [`docs/contest-module-format.md`](docs/contest-module-format.md) for the schema and the existing `contests/*.json` files for examples. If your contest needs a scoring relationship or multiplier type that doesn't exist yet, that's a contest engine change — open an issue first.

### Comments
Default to writing no comments. Only add one when the *why* is non-obvious — a hidden constraint, a workaround for a specific bug, behavior that would surprise a reader. Don't explain *what* the code does (well-named identifiers do that) or reference the current task or PR number.

## Changelog

**Every user-facing change must update [`CHANGELOG.md`](CHANGELOG.md)** under the current unreleased version. Organize entries into these three sections (omit any that don't apply):

1. **Contest Updates** — new contests, contest definition changes
2. **Contest Engine Changes** — scoring/multiplier engine, contest JSON schema additions
3. **Other Changes and Bugfixes** — UI, rig control, file formats, build, everything else

Entries are **release notes**, not commit messages: short, user-facing, and focused on what changed from the operator's perspective. Save deep engine internals and implementation rationale for the commit message.

## Commits & Versioning

- **Version bumps** (across roughly 6 files via `make version <new-version>`) should be **rolled into the same commit** as the substantive change. Do not make standalone `bump 0.x.y` commits.
- Write commit messages that explain **why**, not just what. The diff already shows what.
- Do not add `Co-Authored-By` trailers.

## Pull Requests

Fill out the PR template (it appears automatically when you open a PR). The template asks for a summary, the test commands you ran, and confirmation that `CHANGELOG.md` is updated.

Reviews focus on:
- Cross-platform correctness (does this build on MSVC?)
- Both test suites pass
- Changelog updated and worded as release notes
- No drive-by refactors mixed in with the change

## Reporting Bugs

Open a GitHub issue with:
- ContestLogX version (Help → About, or the footer of the README)
- Platform and OS version
- Steps to reproduce
- Relevant section of `clx_debug.log` (run with `--debug` to capture it). Default location:
  - **Linux:** `~/.local/share/ContestLogX/clx_debug.log`
  - **macOS:** `~/Library/Application Support/ContestLogX/clx_debug.log`
  - **Windows:** `%LOCALAPPDATA%\ContestLogX\clx_debug.log`

## License

By contributing, you agree that your contributions will be licensed under the MIT License (see [`LICENSE`](LICENSE)).
