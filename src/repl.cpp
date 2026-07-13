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
#include "interceptors/format.hpp"
#include "completion.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <unistd.h>
#include <sys/wait.h>
#include <pwd.h>
#include <signal.h>
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
        "  branch -d/-D      creates backup before branch deletion\n"
        "  stash drop/clear  creates backup before dropping stashes\n"
        "  rm                creates backup before removing files\n"
        "  tag -d            creates backup before deleting tags\n"
        "  commit --amend    creates backup before rewriting latest commit\n"
        "  clean -f*         shows preview before executing\n"
        "  reset (soft/mixed) shows how many commits will be discarded\n"
        "  commit -m ''      blocked (empty message)\n"
        "  commit            blocked if unresolved merge conflicts remain\n"
        "  log               pretty with oneline/graph/decorate\n"
        "  diff              colorized diff\n"
        "  show              colorized commit\n"
        "  reflog            colorized reflog\n"
        "  undo              restores your most recent backup\n"
        "  backups           lists all saved backups\n"
        "  config reload     re-reads .whiterose.toml without restart\n"
        "  checkout -        warns if dirty tree\n\n"
        "Beginner aliases (always on):\n"
        "  new-branch <name>  creates a new branch and switches to it\n"
        "  switch <name>      switches to an existing branch\n"
        "  save               stages all changes and commits them\n"
        "  save -m <msg>      save with a message (non-interactive)\n"
        "  sync               pull latest changes from remote\n"
        "  share              push your changes to the remote\n"
        "  teach on/off       toggle plain-language explanations\n\n"
        "Config: place a .whiterose.toml in your repo root to customize\n"
        "protected branches and toggle features. See README for format.\n\n"
        "Exit the shell any time with 'exit' or Ctrl-D.\n";
}

static std::string histfile() {
    struct passwd *pw = getpwuid(getuid());
    return pw ? std::string(pw->pw_dir) + "/.whiterose_history" : "~/.whiterose_history";
}

void run_repl(const std::string &REPO_ROOT) {
    setup_completion();
    read_history(histfile().c_str());
    char *input;
    while ((input = readline(make_prompt(REPO_ROOT.size()).c_str())) != nullptr) {
        std::string line(input);
        free(input);
        if (line == "exit" || line == "quit") { std::cout << "bye\n"; break; }
        if (line == "help") { print_help(true); continue; }
        if (!line.empty()) add_history(line.c_str());

        // config reload
        if (handle_config_command(line, REPO_ROOT)) continue;

        // backups list
        if (handle_backups_command(line)) continue;

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

        // --- safety guards ---
        // git commit -m "" (empty message)
        {
            auto tk = split_words(cmd_line);
            size_t cs = 0;
            if (tk.size() >= 2 && tk[0] == "git") cs = 1;
            if (cs < tk.size() && tk[cs] == "commit") {
                bool has_m = false, empty_m = false;
                for (size_t ti = cs + 1; ti < tk.size(); ti++) {
                    if (tk[ti] == "-m" || tk[ti] == "--message") {
                        has_m = true;
                        if (ti + 1 < tk.size()) {
                            std::string m = tk[ti + 1];
                            if (m.size() >= 2 && m.front() == '"' && m.back() == '"')
                                m = m.substr(1, m.size() - 2);
                            if (m.empty() || m == "") empty_m = true;
                        }
                    }
                }
                if (!has_m && !tk[cs].empty() && tk[cs] == "commit") {
                    // bare commit — check if msg was passed interactively
                    bool via_s = false;
                    for (size_t ti = cs + 1; ti < tk.size(); ti++)
                        if (tk[ti] == "-s" || tk[ti] == "--signoff") via_s = true;
                    if (!via_s) {} // fine, git will open editor
                }
                if (empty_m) {
                    std::cout << "✿ Commit message is empty. Cancelling commit.\n";
                    continue;
                }
            }
        }

        // git checkout - (dash) warning
        {
            auto tk = split_words(cmd_line);
            size_t cs = 0;
            if (tk.size() >= 2 && tk[0] == "git") cs = 1;
            if (cs < tk.size() && tk[cs] == "checkout" && cs + 1 < tk.size() && tk[cs + 1] == "-") {
                std::string dirty = run_capture("git status --porcelain 2>/dev/null");
                if (!dirty.empty()) {
                    std::cout << "✿ You have uncommitted changes — switching away" << std::endl;
                    std::cout << "   with 'checkout -' might cause conflicts if the" << std::endl;
                    std::cout << "   previous branch has a different state of these files.\n";
                }
            }
        }

        // git clean preview
        {
            auto tk = split_words(cmd_line);
            size_t cs = 0;
            if (tk.size() >= 2 && tk[0] == "git") cs = 1;
            if (cs < tk.size() && tk[cs] == "clean") {
                bool has_f = false;
                for (size_t ti = cs + 1; ti < tk.size(); ti++)
                    if (tk[ti][0] == '-' && tk[ti].find('f') != std::string::npos) { has_f = true; break; }
                if (has_f) {
                    std::string dry = run_capture((cmd_line + " -n 2>&1").c_str());
                    if (!dry.empty()) {
                        std::cout << "✿ Would remove:\n";
                        size_t p = 0;
                        while (p < dry.size()) {
                            auto nl = dry.find('\n', p);
                            if (nl != std::string::npos) { std::cout << "  " << dry.substr(p, nl - p) << '\n'; p = nl + 1; }
                            else { std::cout << "  " << dry.substr(p) << '\n'; break; }
                        }
                        std::cout << "✿ Proceed? [y/N] " << std::flush;
                        std::string reply;
                        std::getline(std::cin, reply);
                        if (reply != "y" && reply != "Y") { std::cout << "✿ Cancelled.\n"; continue; }
                    }
                }
            }
        }

        // git reset preview (soft/mixed)
        {
            auto tk = split_words(cmd_line);
            size_t cs = 0;
            if (tk.size() >= 2 && tk[0] == "git") cs = 1;
            if (cs < tk.size() && tk[cs] == "reset" && cs + 1 < tk.size()) {
                bool is_hard = false;
                for (size_t ti = cs + 1; ti < tk.size(); ti++)
                    if (tk[ti] == "--hard") { is_hard = true; break; }
                if (!is_hard) {
                    // Show what commits would be discarded
                    std::string target = tk[cs + 1];
                    // Count commits between target and HEAD
                    std::string cnt = run_capture(("git rev-list --count " + target + "..HEAD 2>/dev/null").c_str());
                    if (!cnt.empty()) {
                        int n = std::stoi(cnt);
                        if (n > 0) {
                            std::cout << "✿ Resetting will discard " << n << " commit(s) after " << target << "." << std::endl;
                            std::cout << "✿ Continue? [y/N] " << std::flush;
                            std::string reply;
                            std::getline(std::cin, reply);
                            if (reply != "y" && reply != "Y") { std::cout << "✿ Cancelled.\n"; continue; }
                        }
                    }
                }
            }
        }

        // backup before destructive commands
        handle_backup(cmd_line);

        // confirm destructive commands (branch -D, stash drop, rm, tag -d, commit --amend)
        if (confirm_destructive(cmd_line)) continue;

        // format interceptors (log, diff, show, reflog)
        std::string fmt_line = cmd_line;
        handle_format_interceptors(cmd_line, fmt_line);
        cmd_line = fmt_line;

        // normalize bare git subcommands
        normalize_bare_git(cmd_line);

        // --- fork + exec ---
        int pfd[2];
        pipe(pfd);
        // ignore SIGINT in parent — child handles it
        struct sigaction sa_old, sa_ign;
        sa_ign.sa_handler = SIG_IGN;
        sigemptyset(&sa_ign.sa_mask);
        sa_ign.sa_flags = 0;
        sigaction(SIGINT, &sa_ign, &sa_old);

        pid_t pid = fork();
        if (pid < 0) {
            sigaction(SIGINT, &sa_old, nullptr);
            std::cerr << "✿ fork failed — skipping command\n";
            close(pfd[0]); close(pfd[1]);
            continue;
        }
        if (pid == 0) {
            signal(SIGINT, SIG_DFL);
            close(pfd[0]);
            dup2(pfd[1], 3);
            close(pfd[1]);
            execlp("/bin/bash", "bash", "-i", "-c",
                (cmd_line + "; echo \"$PWD\" >&3 2>/dev/null").c_str(), nullptr);
            _exit(127);
        }
        close(pfd[1]);
        waitpid(pid, nullptr, 0);
        sigaction(SIGINT, &sa_old, nullptr);

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
    write_history(histfile().c_str());
}
