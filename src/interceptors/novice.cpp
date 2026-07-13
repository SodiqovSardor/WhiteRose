#include "novice.hpp"
#include "../config.hpp"
#include "../git_utils.hpp"
#include <iostream>
#include <string>
#include <vector>

bool handle_teach_command(const std::string &line) {
    std::string cmd = line;
    auto f = cmd.find_first_not_of(" \t");
    if (f != std::string::npos) { auto l = cmd.find_last_not_of(" \t"); cmd = cmd.substr(f, l - f + 1); }
    if (cmd == "teach on") { cfg.novice_mode = true; std::cout << "✿ Novice mode ON — I'll explain what commands do before running them.\n"; return true; }
    if (cmd == "teach off") { cfg.novice_mode = false; std::cout << "✿ Novice mode OFF.\n"; return true; }
    return false;
}

bool handle_aliases(const std::string &line, std::string &new_line) {
    new_line = line;
    std::string cmd = line;
    auto f = cmd.find_first_not_of(" \t");
    if (f != std::string::npos) { auto l = cmd.find_last_not_of(" \t"); cmd = cmd.substr(f, l - f + 1); }
    auto tok = split_words(cmd);
    if (!tok.empty()) {
        if (tok[0] == "new-branch" && tok.size() >= 2) {
            std::cout << "✿ (running: git checkout -b " << tok[1] << ")\n";
            new_line = "git checkout -b " + tok[1];
        } else if (tok[0] == "switch" && tok.size() >= 2) {
            std::cout << "✿ (running: git checkout " << tok[1] << ")\n";
            new_line = "git checkout " + tok[1];
        } else if (tok[0] == "save") {
            std::string msg;
            if (tok.size() >= 3 && tok[1] == "-m") {
                // collect quoted message (may span multiple tokens)
                for (size_t ti = 2; ti < tok.size(); ti++) {
                    if (ti > 2) msg += ' ';
                    msg += tok[ti];
                }
                // strip outer quotes if present
                if (msg.size() >= 2 && msg.front() == '"' && msg.back() == '"')
                    msg = msg.substr(1, msg.size() - 2);
            } else {
                std::cout << "✿ Commit message: " << std::flush;
                std::getline(std::cin, msg);
                if (msg.empty()) { std::cout << "✿ Commit cancelled — message required.\n"; return true; }
            }
            for (size_t i = 0; i < msg.size(); i++)
                if (msg[i] == '"' || msg[i] == '\\') { msg.insert(i, 1, '\\'); i++; }
            std::cout << "✿ (running: git add -A && git commit -m \"" << msg << "\")\n";
            new_line = "git add -A && git commit -m \"" + msg + "\"";
        } else if (tok[0] == "sync") {
            std::cout << "✿ (running: git pull)\n";
            new_line = "git pull";
        } else if (tok[0] == "share") {
            std::cout << "✿ (running: git push)\n";
            new_line = "git push";
        }
    }
    return false;
}

void explain_command(const std::string &line) {
    if (!cfg.novice_mode) return;
    std::string cmd = line;
    auto f = cmd.find_first_not_of(" \t");
    if (f != std::string::npos) { auto l = cmd.find_last_not_of(" \t"); cmd = cmd.substr(f, l - f + 1); }
    auto tok = split_words(cmd);
    size_t s = 0;
    if (tok.size() >= 2 && tok[0] == "git") s = 1;
    if (s < tok.size()) {
        const auto &c = tok[s];
        if (c == "checkout" && s + 1 < tok.size() && tok[s+1] == "-b" && s + 2 < tok.size())
            std::cout << "✿ Creating a new branch called '" << tok[s+2] << "' — your own space to make changes without affecting others.\n";
        else if (c == "checkout" && s + 1 < tok.size() && tok[s+1][0] != '-')
            std::cout << "✿ Switching to branch '" << tok[s+1] << "'.\n";
        else if (c == "branch" && s + 1 < tok.size() && tok[s+1][0] != '-')
            std::cout << "✿ Creating a new branch called '" << tok[s+1] << "' — your own space to make changes without affecting others.\n";
        else if (c == "add") {
            std::string p = (s + 1 < tok.size()) ? tok[s+1] : ".";
            std::cout << "✿ Staging " << p << " — marking it ready to be included in your next commit.\n";
        } else if (c == "commit")
            std::cout << "✿ Saving a snapshot of your staged changes with a message.\n";
        else if (c == "push" && s + 1 >= tok.size())
            std::cout << "✿ Uploading your commits to the shared remote repository.\n";
        else if (c == "pull" && s + 1 >= tok.size())
            std::cout << "✿ Downloading and merging the latest changes from the remote.\n";
        else if (c == "merge" && s + 1 < tok.size())
            std::cout << "✿ Combining changes from '" << tok[s+1] << "' into your current branch.\n";
    }
}

static bool is_git_subcommand(const std::string &word) {
    return (word == "checkout" || word == "merge" || word == "rebase" ||
            word == "branch" || word == "add" || word == "commit" ||
            word == "log" || word == "diff" || word == "show" ||
            word == "reset" || word == "clean" || word == "fetch" ||
            word == "remote" || word == "stash" || word == "tag");
}

void normalize_bare_git(std::string &line) {
    std::string cmd = line;
    auto f = cmd.find_first_not_of(" \t");
    if (f != std::string::npos) { auto l = cmd.find_last_not_of(" \t"); cmd = cmd.substr(f, l - f + 1); }
    if (cmd.compare(0, 4, "git ") != 0) {
        auto tok = split_words(cmd);
        if (!tok.empty() && is_git_subcommand(tok[0]))
            line = "git " + line.substr(line.find_first_not_of(" \t"));
    }
}
