# whiterose ✿

**A git-aware safety shell.** Boots inside any git repo, wraps your shell with smart guards — auto-upstream on push, force-push protection, undo for destructive commands, lockfile conflict auto-fix, and more.

## Features

| Feature | What it does |
|---------|-------------|
| **Boot guard** | Run `whiterose` inside any git repo — it finds the repo root, tracks your `cd` and branch, and keeps a clean prompt |
| **Push auto-upstream** | Bare `git push` on a branch with no upstream → automatically adds `--set-upstream origin <branch>` |
| **Force-push protection** | `git push --force` checks if remote has commits you don't. Blocks with a warning if force-pushing would destroy history |
| **Protected branch guard** | Pushing directly to `main`/`master` (or your configured list) requires confirmation |
| **Backup + undo** | Before `reset --hard`, `checkout --`, `clean -fd`, `push --force`, or `rebase` — saves a backup ref. `undo` restores it, including uncommitted and untracked files |
| **Smart pull** | Bare `git pull` on a clean tree → `git pull --rebase` instead of merge. Dirty tree → falls back to regular merge pull |
| **Status reformat** | `git status` → color-coded summary (staged/modified/untracked/conflicts) instead of raw output |
| **Lockfile auto-fix** | After a merge conflict, if a lockfile (`package-lock.json`, `yarn.lock`, etc.) is conflicted → offers to regenerate it automatically |
| **Per-repo config** | `.whiterose.toml` at repo root to customize protected branches and toggle features |

## Quick start

```bash
# Build
g++ -std=c++17 -Os -s main.cpp -o whiterose -lreadline

# Or via Makefile
make
sudo make install   # installs to /usr/local/bin
make install-user   # installs to ~/.local/bin
```

Then inside any git repo:

```bash
whiterose
✿ whiterose booted at /home/you/project
✿ project (main) ❯ git push
✿ no upstream set — pushing with --set-upstream origin main
# ... push goes through automatically ...
✿ project (main) ❯ git push --force
✿ Blocked: remote has 2 commit(s) you don't have locally.
   Force-pushing would destroy them. Use 'push --force-with-lease'
   if you're sure, or pull first.
✿ project (main) ❯ git reset --hard HEAD~1
✿ backup saved: 20260712-235959
# ... HEAD is now at abc1234 ...
✿ project (main) ❯ undo
✿ Restoring to backup 20260712-235959 (commit def5678)
✿ Restored.
✿ project (main) ❯ exit
bye
```

## Install

### Dependencies
- C++17 compiler (g++ ≥ 7 or clang ≥ 5)
- libreadline (usually `libreadline-dev` on Debian/Ubuntu, `readline-devel` on Fedora)
- bash, git

### Build

```bash
git clone https://github.com/yourusername/whiterose.git
cd whiterose
make
```

### Linux (system-wide)
```bash
sudo make install   # → /usr/local/bin/whiterose
```

### Linux (user-local)
```bash
make install-user   # → ~/.local/bin/whiterose
# ensure ~/.local/bin is on your PATH
```

### macOS
```bash
make install-mac    # → /usr/local/bin
# or
make install-user   # → ~/.local/bin
```

## Configuration

`.whiterose.toml` at repo root:

```toml
# .whiterose.toml
protected_branches = ["main", "develop", "release"]
auto_upstream = true
smart_pull = true
backup_on_destructive = true
```

All keys optional. Missing file → built-in defaults.

## Usage tips

- **Tab completion** — works out of the box via readline
- **History** — up/down arrows cycle through commands
- **Ctrl-D** to exit
- **`exit`** or **`quit`** to exit
- **`undo`** — restore the last automatic backup
- **`status`**, **`st`**, or **`git status`** — shows the reformatted summary
- **`push`** alone works (aliased to `git push` internally)
- **`pull`** alone works with smart rebase-if-safe

## Architecture

```
whiterose
├── boot guard         → detect git repo, load config
├── readline REPL      → input with tab completion + history
├── interceptors       → push, pull, status, destructive commands
├── fd-3 pipe          → track cwd after every command
└── bash -i -c         → execute each command in real bash
```

Every command passes through C++ code first — this is the hook point for all safety features. No `exec()` into a shell replacement; C++ stays alive as the controller.

## License

MIT
