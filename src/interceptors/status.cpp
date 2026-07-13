#include "status.hpp"
#include "../git_utils.hpp"
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

static void parseAndPrintStatus() {
    FILE *fp = popen("git status --porcelain=v2 --branch 2>/dev/null", "r");
    if (!fp) { std::cout << "✿ could not run git status\n"; return; }

    std::string branch, upstream;
    int ahead = 0, behind = 0;
    std::vector<std::string> staged, modified, untracked, conflicts;

    char line[4096];
    while (fgets(line, sizeof(line), fp)) {
        size_t n = strlen(line);
        while (n > 0 && (line[n-1] == '\n' || line[n-1] == '\r')) line[--n] = '\0';
        if (n == 0) continue;
        if (line[0] == '#') {
            char val[1024] = {};
            if (sscanf(line, "# branch.head %1023[^\n]", val) == 1) branch = val;
            else if (sscanf(line, "# branch.upstream %1023[^\n]", val) == 1) upstream = val;
            else if (sscanf(line, "# branch.ab %d %d", &ahead, &behind) >= 1) ;
        } else if (line[0] == '?' && line[1] == ' ') {
            untracked.push_back(line + 2);
        } else if ((line[0] == '1' || line[0] == '2') && line[1] == ' ') {
            char X = line[2], Y = line[3];
            int to_skip = (line[0] == '1') ? 7 : 8;
            int count = 0;
            char *p = line + 2;
            while (*p && count < to_skip) {
                while (*p == ' ') p++;
                if (!*p) break;
                while (*p && *p != ' ') p++;
                count++;
            }
            while (*p == ' ') p++;
            std::string path(p);
            if (line[0] == '2') {
                char *sp = strchr(p, ' ');
                if (sp) { *sp = '\0'; path = p; }
            }
            if (X != '.' && X != '?' && X != ' ') staged.push_back(path);
            if (Y != '.' && Y != ' ') modified.push_back(path);
        } else if (line[0] == 'u' && line[1] == ' ') {
            int count = 0;
            char *p = line + 2;
            while (*p && count < 9) {
                while (*p == ' ') p++;
                if (!*p) break;
                while (*p && *p != ' ') p++;
                count++;
            }
            while (*p == ' ') p++;
            conflicts.push_back(p);
        }
    }
    pclose(fp);

    std::cout << "\x1b[1;37m✿\x1b[0m";
    if (!branch.empty()) {
        std::cout << " \x1b[1;34m" << branch << "\x1b[0m";
        if (!upstream.empty() && upstream != ".") {
            auto sp = upstream.rfind('/');
            std::string u = (sp != std::string::npos) ? upstream.substr(sp + 1) : upstream;
            std::cout << " \x1b[0;90m(" << u << ")\x1b[0m";
        }
        if (ahead > 0) std::cout << " \x1b[1;32m↑" << ahead << "\x1b[0m";
        if (behind > 0) std::cout << " \x1b[1;31m↓" << behind << "\x1b[0m";
    }
    std::cout << std::endl;

    if (staged.empty() && modified.empty() && untracked.empty() && conflicts.empty()) {
        std::cout << "\x1b[1;32m✿ working tree clean\x1b[0m" << std::endl;
        return;
    }
    if (!conflicts.empty()) {
        std::cout << "\x1b[1;31mConflicts:\x1b[0m\n";
        for (auto &f : conflicts) std::cout << "  " << f << '\n';
    }
    if (!staged.empty()) {
        std::cout << "\x1b[1;32mStaged:\x1b[0m\n";
        for (auto &f : staged) std::cout << "  " << f << '\n';
    }
    if (!modified.empty()) {
        std::cout << "\x1b[1;33mModified:\x1b[0m\n";
        for (auto &f : modified) std::cout << "  " << f << '\n';
    }
    if (!untracked.empty()) {
        std::cout << "\x1b[0;90mUntracked:\x1b[0m\n";
        for (auto &f : untracked) std::cout << "  " << f << '\n';
    }
}

bool handle_status_interceptors(const std::string &line) {
    std::string cmd = line;
    auto f = cmd.find_first_not_of(" \t");
    if (f != std::string::npos) { auto l = cmd.find_last_not_of(" \t"); cmd = cmd.substr(f, l - f + 1); }
    if (cmd == "status" || cmd == "st" || cmd == "git status") {
        parseAndPrintStatus();
        return true;
    }
    return false;
}
