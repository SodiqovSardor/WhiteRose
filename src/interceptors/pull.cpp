#include "pull.hpp"
#include "../config.hpp"
#include "../git_utils.hpp"
#include <iostream>
#include <string>

bool handle_pull_interceptors(const std::string &line, std::string &new_line) {
    new_line = line;
    std::string cmd = line;
    auto f = cmd.find_first_not_of(" \t");
    if (f != std::string::npos) { auto l = cmd.find_last_not_of(" \t"); cmd = cmd.substr(f, l - f + 1); }
    if (cmd.compare(0, 4, "git ") != 0 && cmd.compare(0, 4, "pull") == 0)
        new_line = "git " + new_line.substr(new_line.find_first_not_of(" \t"));

    if (cfg.smart_pull && (cmd == "pull" || cmd == "git pull")) {
        std::string up = run_capture("git rev-parse --abbrev-ref --symbolic-full-name @{u} 2>/dev/null");
        if (!up.empty()) {
            std::string dirty = run_capture("git status --porcelain 2>/dev/null");
            if (!dirty.empty()) {
                std::cout << "✿ Uncommitted changes present — using regular pull (merge)" << std::endl;
                std::cout << "   to avoid rebase conflicts. Commit or stash first" << std::endl;
                std::cout << "   if you want a clean rebase." << std::endl;
                new_line = "git pull";
            } else {
                std::cout << "✿ pulling with --rebase (clean working tree)" << std::endl;
                new_line = "git pull --rebase";
            }
        }
    }
    return false;
}
