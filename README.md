<div align="center">
  <img src="logo.svg" width="110" height="110" alt="WhiteRose logo" />

  # whiterose ✿

  [![build](https://github.com/SodiqovSardor/WhiteRose/actions/workflows/build.yml/badge.svg)](https://github.com/SodiqovSardor/WhiteRose/actions/workflows/build.yml)

  **The git shell with an undo button for the mistakes that normally ruin your day.**

  <br/>

  ![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C?style=flat-square&logo=cplusplus)
  ![License](https://img.shields.io/badge/license-MIT-3DA639?style=flat-square)
  ![Platform](https://img.shields.io/badge/platform-linux%20%7C%20macos-6e6e6e?style=flat-square)
  ![Status](https://img.shields.io/badge/status-active-4CAF50?style=flat-square)

</div>

<br/>

> Git is powerful and merciless. One wrong `reset --hard`, one bad force-push, one accidental merge into `main` — and it's gone. **whiterose** is a native C++ shell that lives inside your repo and quietly protects you from the git mistakes that actually cost people work.

<br/>

## Why whiterose

<table>
<tr>
<td width="50%" valign="top">

### 🛡️ Real undo, not a gimmick
Before any destructive command, whiterose silently snapshots your state — including uncommitted and untracked files. Type `undo`. You're back. No other daily-driver shell gives you this.

</td>
<td width="50%" valign="top">

### 🧠 Smart by default
`push` sets its own upstream. `pull` rebases when it's safe to, merges when it isn't. Force-pushes that would destroy a teammate's work get blocked before they happen.

</td>
</tr>
<tr>
<td width="50%" valign="top">

### 🌱 Built for teaching, not just protecting
Flip on `teach on` and every command explains itself in plain English — built for the teammate who's never branched, committed, or pushed before.

</td>
<td width="50%" valign="top">

### 🪶 Zero bloat
Pure C++17. No runtime, no heavy dependencies — just `readline`, `bash`, and `git`. Feels instant because it is instant.

</td>
</tr>
</table>

<br/>

## See it in action

```
$ whiterose
✿ whiterose v1.1.1 booted at ~/project
✿ project (main) ❯ push
✿ no upstream set — pushing with --set-upstream origin main

✿ project (main) ❯ push --force
✿ Blocked: remote has 2 commit(s) you don't have locally.
   Force-pushing would destroy them. Use --force-with-lease if you're sure.

✿ project (main) ❯ reset --hard HEAD~1
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

✿ project (main) ❯ teach on
✿ Novice mode ON — I'll explain what commands do before running them.

✿ project (main) ❯ new-branch feature/login
✿ Creating a new branch called 'feature/login' — your own space to
   make changes without affecting others.
✿ (running: git checkout -b feature/login)
```

<br/>

## Features

| Feature | What it does |
|---|---|
| **Boot guard** | Only runs inside a git repo. Finds the repo root and keeps a live prompt with branch + dirty-file count. |
| **Push auto-upstream** | Bare `push` on a branch with no upstream → auto `--set-upstream origin <branch>`. |
| **Force-push protection** | Checks if the remote has commits you don't before allowing `--force`. Blocks with a warning if it would destroy work. |
| **Protected branch guard** | Pushing straight to `main`/`master` (or your configured list) requires a confirmation. |
| **Backup + undo** | Snapshots state before `reset --hard`, `checkout --`, `clean -fd`, `push --force`, or `rebase` — including uncommitted *and* untracked files. `undo` restores everything. |
| **Smart pull** | Bare `pull` on a clean tree rebases automatically. Dirty tree → falls back to a normal merge pull. |
| **Status reformat** | `status` → a clean, color-coded summary instead of raw git output. |
| **Lockfile auto-fix** | Merge conflicts in `package-lock.json`, `yarn.lock`, `Cargo.lock`, etc. can be auto-regenerated instead of hand-merged. |
| **Conflict viewer** | Real conflicts get a readable, boxed side-by-side view — HEAD vs incoming, hunk by hunk. |
| **Commit guard** | Blocks `commit` while unresolved conflict markers remain. |
| **Novice mode** | `teach on` explains every command in plain English — built for onboarding teammates new to git. |
| **Beginner aliases** | `new-branch`, `switch`, `save`, `sync`, `share` — friendly names that map to real git commands, shown alongside the real syntax so it's a learning tool, not a crutch. |
| **Per-repo config** | Drop a `.whiterose.toml` in your repo root to customize protected branches and toggle any feature. |

<br/>

## Install

**Dependencies**
- A C++17 compiler (`g++` ≥ 7 or `clang` ≥ 5)
- `libreadline` (`libreadline-dev` on Debian/Ubuntu, `readline-devel` on Fedora, `readline` via Homebrew on macOS)
- `bash` and `git`

**Build from source**

```bash
git clone https://github.com/SodiqovSardor/WhiteRose.git
cd WhiteRose
make
make install-user   # → ~/.local/bin/whiterose
```

Make sure `~/.local/bin` is on your `PATH`.

<br/>

## Configuration

Drop a `.whiterose.toml` in your repo root — every key is optional, missing file means built-in defaults.

```toml
protected_branches = ["main", "develop", "release"]
auto_upstream       = true
smart_pull          = true
backup_on_destructive = true
novice_mode          = false
```

<br/>

## Architecture

```
whiterose
├── boot guard         → detect git repo, load config
├── readline REPL       → input with tab completion + history
├── interceptors        → push, pull, status, destructive commands,
│                          conflicts, novice mode
├── fd-3 pipe            → tracks cwd after every command
└── bash -i -c            → executes each command in real bash
```

Every command passes through whiterose's C++ core first — that's the hook point for every safety feature. There's no `exec()` into a shell replacement; the controller process stays alive the entire session.

<details>
<summary><strong>Why not just <code>exec()</code> into bash?</strong></summary>
<br/>

Because `exec()` replaces the current process entirely — once you're inside real bash, there's no C++ code left in the loop to intercept anything. whiterose instead `fork()`s a subprocess per command and waits for it to finish, so control always returns to the C++ layer before the next prompt. That's what makes undo, force-push protection, and everything else in this list possible.
</details>

<br/>

## FAQ

<details>
<summary><strong>Does this replace git?</strong></summary>
<br/>
No. Every command still runs through real git under the hood — whiterose adds guardrails and shortcuts, it doesn't reimplement anything.
</details>

<details>
<summary><strong>What happens to <code>undo</code> backups over time?</strong></summary>
<br/>
Backups are stored as ordinary git refs under <code>refs/whiterose/</code> and auto-prune to the 20 most recent, so they won't accumulate indefinitely.
</details>

<details>
<summary><strong>Can real merge conflicts be auto-resolved?</strong></summary>
<br/>
No — and whiterose won't pretend to. Only mechanical, low-risk cases are auto-fixed (lockfiles, non-overlapping changes). Genuine conflicting edits always get a clean view for you to resolve by hand.
</details>

<br/>

## License

MIT

<br/>

<div align="center">
  <sub>Built with ✿ by <a href="https://github.com/SodiqovSardor">Sardor</a></sub>
</div>
