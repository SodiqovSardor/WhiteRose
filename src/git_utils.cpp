#include "git_utils.hpp"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

std::vector<std::string> split_words(const std::string &s) {
    std::vector<std::string> r;
    size_t i = 0;
    while (i < s.size()) {
        if (s[i] == ' ') { i++; continue; }
        size_t j = s.find_first_of(' ', i);
        r.push_back(s.substr(i, j - i));
        i = j;
    }
    return r;
}

std::string run_capture(const char *cmd) {
    FILE *fp = popen(cmd, "r");
    if (!fp) return {};
    std::string out;
    char buf[256];
    while (fgets(buf, sizeof(buf), fp)) out += buf;
    if (pclose(fp) != 0) return {};
    while (!out.empty() && (out.back() == '\n' || out.back() == '\r'))
        out.pop_back();
    return out;
}

bool cmd_ok(const char *cmd) {
    FILE *fp = popen(cmd, "r");
    if (!fp) return false;
    char buf[256];
    while (fgets(buf, sizeof(buf), fp));
    return pclose(fp) == 0;
}

std::string get_current_branch() {
    return run_capture("git branch --show-current 2>/dev/null");
}
