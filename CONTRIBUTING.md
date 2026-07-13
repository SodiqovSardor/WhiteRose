# Contributing to WhiteRose

Thanks for wanting to help! WhiteRose is a small project built to make git safer for everyone, including people who don't know git well yet. Every contribution that serves that goal is welcome.

## Getting started

```bash
git clone https://github.com/SodiqovSardor/WhiteRose.git
cd WhiteRose
make
```

Dependencies: C++17 compiler (g++ ≥ 7 or clang ≥ 5), libreadline, bash, git.

## How to contribute

### Report a bug
Open an issue with:
- What you typed
- What happened
- What you expected to happen
- Your OS, compiler version, and git version

### Suggest a feature
Open an issue describing the problem you're solving, not just the solution you want. WhiteRose has a clear design philosophy — C++ stays alive as the controller, every command passes through interception hooks, and safety nets never silently disable. Features that fit that philosophy are more likely to be accepted.

### Submit a pull request
1. Fork the repo.
2. Create a branch: `git checkout -b feature/your-feature`.
3. Make your changes.
4. Test your changes: `make && echo 'status' | timeout 2 ./whiterose` inside a git repo.
5. Commit and push.
6. Open a pull request.

## Code style

- C++17. No external dependencies beyond libreadline.
- Single responsibility per file. Interceptors live in `src/interceptors/`.
- No `exec()` into a shell replacement — C++ stays alive as the controller.
- Every feature that adds a safety net needs a test case logged. See `Logs/` and the existing verification tables for the format.
- Keep it simple. If a feature needs paragraphs of explanation, it probably doesn't fit.

## Design principles

1. **C++ stays alive.** `fork()` + `execvp()` loop, never `exec()` into a shell.
2. **Fail safe.** If a safety check can't complete, warn and allow — never silently block or silently allow.
3. **Compose, don't bypass.** Aliases and new features go through the same interception chain as raw commands.
4. **Config over hardcode.** New behavior should be toggleable via `.whiterose.toml`.
5. **Untracked files matter.** Backups capture staged, unstaged, and untracked changes.

## Questions

Open an issue. No question is too basic.
