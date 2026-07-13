# whiterose ✿

**A git-aware safety shell.** Boots inside any git repo, wraps your shell with smart guards — auto-upstream on push, force-push protection, undo for destructive commands, lockfile conflict auto-fix, conflict viewer, commit guard, and more.

## Features

| Feature | What it does |
|---------|-------------|
| **Boot guard** | Run `whiterose` inside any git repo — finds repo root, tracks `cd` and branch, keeps a clean prompt |
| **Push auto-upstream** | Bare `push` on a branch with no upstream → auto `--set-upstream origin <branch>` |
| **Force-push protection** | `git push --force` checks if remote has commits you don't. Blocks with warning |
| **Protected branch guard** | Pushing directly to `main`/`master` (or your configured list) requires confirmation |
| **Backup + undo** | Before `reset --hard`, `checkout --`, `clean -fd`, `push --force`, or `rebase` — saves backup refs. `undo` restores everything, including uncommitted and untracked files |
| **Smart pull** | Bare `pull` on clean tree → `git pull --rebase`. Dirty tree → merge fallback |
| **Status reformat** | `git status` → color-coded summary (staged/modified/untracked/conflicts) |
| **Lockfile auto-fix** | Merge conflict on `package-lock.json` / `yarn.lock` / etc. → offers auto-regeneration |
| **Conflict viewer** | After a merge, shows boxed view of each hunk: `HEAD (yours)` vs `incoming` |
| **Commit guard** | `git commit` with unresolved conflicts → blocked with warning |
| **Per-repo config** | `.whiterose.toml` to customize protected branches and toggle features |

## Quick start

```bash
# Build
g++ -std=c++17 -Os -s main.cpp -o whiterose -lreadline
```

Inside any git repo:

```bash
whiterose
✿ whiterose booted at ~/project
✿ project (main) ❯ git push
✿ no upstream set — pushing with --set-upstream origin main

✿ project (main) ❯ git push --force
✿ Blocked: remote has 2 commit(s) you don't have locally.
   Force-pushing would destroy them.

✿ project (main) ❯ git reset --hard HEAD~1
✿ backup saved: 20260712-235959
✿ project (main) ❯ undo
✿ Restoring to backup 20260712-235959 (commit def5678)
✿ Restored.

✿ project (main) ❯ status
✿ main
  Staged:
    index.html
  Modified:
    style.css
  Untracked:
    newfile.txt

✿ project (main) ❯ git merge feature
✿ Conflict in file.txt (hunk 1 of 2)
┌─ HEAD (yours) ─────────────────────────
│ existing code
├─ incoming ─────────────────────────────
│ new feature code
└────────────────────────────────────────

✿ project (main) ❯ git commit -m "done"
✿ Blocked: 1 unresolved conflict(s) remain in file.txt.
   Resolve and 'git add' them before committing.

✿ project (main) ❯ exit
bye
```

## Install

### Dependencies
- C++17 compiler (g++ ≥ 7 or clang ≥ 5)
- [libreadline](https://tiswww.case.edu/php/chet/readline/rltop.html) (`libreadline-dev` on Debian/Ubuntu, `readline-devel` on Fedora, `readline` on macOS via Homebrew)
- bash, git

### Build

```bash
git clone https://github.com/SodiqovSardor/WhiteRose.git
cd WhiteRose
make
make install-user   # → ~/.local/bin/whiterose
```

Or just copy the binary:

```bash
g++ -std=c++17 -Os -s main.cpp -o ~/.local/bin/whiterose -lreadline
```

## Configuration

Create `.whiterose.toml` at repo root:

```toml
protected_branches = ["main", "develop", "release"]
auto_upstream = true
smart_pull = true
backup_on_destructive = true
```

All keys optional. Missing file → built-in defaults.

## Architecture

```
whiterose
├── boot guard         → detect git repo, load config
├── readline REPL      → input with tab completion + history
├── interceptors       → push, pull, status, destructive commands
├── fd-3 pipe          → track cwd after every command
└── bash -i -c         → execute each command in real bash
```

Every command passes through C++ code first — the hook point for all safety features. No `exec()` into a shell replacement; C++ stays alive as the controller.

## License

MIT
