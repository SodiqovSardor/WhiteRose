#include "format.hpp"
#include "../git_utils.hpp"
#include <string>
#include <vector>

bool handle_format_interceptors(const std::string &line, std::string &new_line) {
    new_line = line;
    std::string cmd = line;
    auto f = cmd.find_first_not_of(" \t");
    if (f != std::string::npos) { auto l = cmd.find_last_not_of(" \t"); cmd = cmd.substr(f, l - f + 1); }
    auto tok = split_words(cmd);
    if (tok.empty()) return false;

    // only intercept bare commands — if user passes flags, let them through
    size_t s = 0;
    if (tok.size() >= 2 && tok[0] == "git") s = 1;
    if (s >= tok.size()) return false;
    const std::string &c = tok[s];

    // "bare" = exactly one arg beyond optional "git" prefix
    bool bare = (tok.size() == (s + 1));

    if (c == "log" && bare) {
        new_line = "git log --oneline --graph --decorate --all --color=always 2>/dev/null | head -$((LINES-3))";
        return true;
    }
    if (c == "diff" && bare) {
        new_line = "git diff --color=always 2>/dev/null";
        return true;
    }
    if (c == "show" && bare) {
        new_line = "git show --color=always 2>/dev/null";
        return true;
    }
    if (c == "reflog" && bare) {
        new_line = "git reflog --color=always 2>/dev/null | head -30";
        return true;
    }
    return false;
}
