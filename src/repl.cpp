#include "repl.hpp"
#include "boot.hpp"
#include "config.hpp"
#include "git_utils.hpp"
#include "interceptors/push.hpp"
#include "interceptors/pull.hpp"
#include "interceptors/status.hpp"
#include "interceptors/destructive.hpp"
#include "interceptors/conflicts.hpp"
#include "interceptors/novice.hpp"
#include "completion.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <unistd.h>
#include <sys/wait.h>
#include <readline/readline.h>
#include <readline/history.h>

namespace fs = std::filesystem;

static std::string git_branch() {
    FILE *fp = popen("git rev-parse --abbrev-ref HEAD 2>/dev/null", "r");
    if (!fp) return {};
    char buf[64] = {};
    fgets(buf, sizeof(buf), fp);
    pclose(fp);
    std::string s(buf);
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
    if (s.empty() || s == "HEAD") return {};
    return " (" + s + ")";
}

std::string make_prompt(int repo_root_len) {
    (void)repo_root_len;
    std::string dir = fs::current_path().filename().string();
    std::string br = git_branch();
    return "\001\x1b[1;37m\002✿ \001\x1b[1;34m\002" + dir
         + "\001\x1b[0;90m\002" + br
         + "\001\x1b[0m\002 \001\x1b[1;32m\002❯\001\x1b[0m\002 ";
}

void print_help(bool inside) {
    std::cout << "whiterose v" << WHITEROSE_VERSION << " — a git-aware shell that protects you from\n"
        "destructive mistakes.\n\n";
    if (!inside) std::cout << "Usage: whiterose (run inside a git repository)\n\n";
    std::cout << "Intercepted behavior:\n"
        "  push              auto-sets upstream if missing\n"
        "  push --force/-f   blocked if remote has commits you don't have\n"
        "                     (use --force-with-lease if you're sure)\n"
        "  pull              uses --rebase automatically when working tree\n"
        "                     is clean, falls back to merge if dirty\n"
        "  status / st       clean, color-coded summary\n"
        "  reset --hard,\n"
        "  checkout --,\n"
        "  clean -f*,\n"
        "  rebase            creates an automatic backup before running\n"
        "  undo              restores your most recent backup\n"
        "  commit            blocked if unresolved merge conflicts remain\n\n"
        "Beginner aliases (always on):\n"
        "  new-branch <name>  creates a new branch and switches to it\n"
        "  switch <name>      switches to an existing branch\n"
        "  save               stages all changes and commits them\n"
        "  sync               pull latest changes from remote\n"
        "  share              push your changes to the remote\n"
        "  teach on/off       toggle plain-language explanations\n\n"
        "Config: place a .whiterose.toml in your repo root to customize\n"
        "protected branches and toggle features. See README for format.\n\n"
        "Exit the shell any time with 'exit' or Ctrl-D.\n";
}

void run_repl(const std::string &REPO_ROOT) {
    setup_completion();
    char *input;
    while ((input = readline(make_prompt(REPO_ROOT.size()).c_str())) != nullptr) {
        std::string line(input);
        free(input);
        if (line == "exit" || line == "quit") { std::cout << "bye\n"; break; }
        if (line == "help") { print_help(true); continue; }
        if (!line.empty()) add_history(line.c_str());

        // teach on/off
        if (handle_teach_command(line)) continue;

        // aliases (may rewrite line)
        std::string cmd_line = line;
        if (handle_aliases(line, cmd_line)) continue;

        // novice explanations
        explain_command(cmd_line);

        // status intercept
        if (handle_status_interceptors(cmd_line)) continue;

        // push interceptors (force-push, protected branch, auto-upstream)
        std::string push_line = cmd_line;
        if (handle_push_interceptors(cmd_line, push_line)) continue;
        cmd_line = push_line;

        // pull interceptor
        std::string pull_line = cmd_line;
        handle_pull_interceptors(cmd_line, pull_line);
        cmd_line = pull_line;

        // undo
        if (handle_undo_command(cmd_line)) continue;

        // commit guard
        if (handle_commit_guard(cmd_line)) continue;

        // backup before destructive commands
        handle_backup(cmd_line);

        // normalize bare git subcommands
        normalize_bare_git(cmd_line);

        // --- fork + exec ---
        int pfd[2];
        pipe(pfd);
        pid_t pid = fork();
        if (pid < 0) {
            std::cerr << "✿ fork failed — skipping command\n";
            close(pfd[0]); close(pfd[1]);
            continue;
        }
        if (pid == 0) {
            close(pfd[0]);
            dup2(pfd[1], 3);
            close(pfd[1]);
            execlp("/bin/bash", "bash", "-i", "-c",
                (cmd_line + "; echo \"$PWD\" >&3 2>/dev/null").c_str(), nullptr);
            _exit(127);
        }
        close(pfd[1]);
        waitpid(pid, nullptr, 0);

        char buf[4096];
        ssize_t n = read(pfd[0], buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            std::string d(buf);
            while (!d.empty() && (d.back() == '\n' || d.back() == '\r')) d.pop_back();
            if (!d.empty()) chdir(d.c_str());
        }
        close(pfd[0]);

        // conflict check after pull/merge/rebase
        if (is_pull_merge_rebase(cmd_line))
            check_lockfile_conflicts(REPO_ROOT);
    }
}
