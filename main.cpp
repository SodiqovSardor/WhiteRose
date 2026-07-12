// whiterose v0.0.1 — boot guard + fork/execvp passthrough REPL

#include <iostream>
#include <string>
#include <filesystem>
#include <unistd.h>
#include <sys/wait.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <cstdio>

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

static std::string make_prompt() {
    std::string dir = fs::current_path().filename().string();
    std::string br = git_branch();
    // \001 / \002 mark non-printing chars so readline measures width correctly
    return "\001\x1b[1;34m\002✿ " + dir
         + "\001\x1b[0;90m\002" + br
         + "\001\x1b[0m\002 \001\x1b[1;32m\002❯\001\x1b[0m\002 ";
}

static std::string find_repo_root() {
    fs::path p = fs::current_path();
    for (;;) {
        if (fs::exists(p / ".git")) return p.string();
        if (p == p.root_path()) break;
        p = p.parent_path();
    }
    return {};
}

int main() {
    std::string REPO_ROOT = find_repo_root();
    if (REPO_ROOT.empty()) {
        std::cout << "✿ White Rose: Crimson error. Not a git repository.\n";
        return 1;
    }
    std::cout << "✿ whiterose booted at " << REPO_ROOT << std::endl;

    char *input;
    while ((input = readline(make_prompt().c_str())) != nullptr) {
        std::string line(input);
        free(input);

        if (line == "exit" || line == "quit") {
            std::cout << "bye\n";
            break;
        }

        if (!line.empty()) add_history(line.c_str());

        pid_t pid = fork();
        if (pid == 0) {
            execlp("/bin/bash", "bash", "-i", "-c", line.c_str(), nullptr);
            _exit(127);
        }
        if (pid > 0) waitpid(pid, nullptr, 0);
    }
}
// g++ -std=c++17 -Os -s main.cpp -o whiterose -lreadline
