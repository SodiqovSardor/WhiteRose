// whiterose v1.1.0 — git-aware shell with destructive-command protection, smart defaults, conflict tooling, and novice mode

#include "src/config.hpp"
#include "src/boot.hpp"
#include "src/git_utils.hpp"
#include "src/repl.hpp"
#include <iostream>
#include <string>

int main(int argc, char **argv) {
    if (argc > 1) {
        std::string flag(argv[1]);
        if (flag == "--version" || flag == "-v") { std::cout << "whiterose v" << WHITEROSE_VERSION << std::endl; return 0; }
        if (flag == "--help" || flag == "-h") { print_help(false); return 0; }
    }

    std::string REPO_ROOT = find_repo_root();
    if (REPO_ROOT.empty()) {
        std::cout << "✿ White Rose: Crimson error. Not a git repository.\n";
        return 1;
    }

    std::cout << "✿ whiterose v" << WHITEROSE_VERSION << " booted at " << REPO_ROOT << std::endl;
    cfg = load_config(REPO_ROOT);

    run_repl(REPO_ROOT);
}
