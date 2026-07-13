#include "conflicts.hpp"
#include "../config.hpp"
#include "../git_utils.hpp"
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
#include <utility>

static const std::vector<std::pair<std::string, std::string>> lockfiles = {
    {"package-lock.json", "npm install"},
    {"yarn.lock", "yarn install"},
    {"pnpm-lock.yaml", "pnpm install"},
    {"Cargo.lock", "cargo generate-lockfile"},
    {"composer.lock", "composer update --lock"},
};

static const std::vector<std::pair<std::string, std::string>> lockfile_manifests = {
    {"package-lock.json", "package.json"},
    {"yarn.lock", "package.json"},
    {"pnpm-lock.yaml", "package.json"},
    {"Cargo.lock", "Cargo.toml"},
    {"composer.lock", "composer.json"},
};

static std::vector<std::string> shown_conflicts;

static void parse_hunks(const std::string &content, std::vector<std::pair<std::string, std::string>> &res) {
    size_t pos = 0;
    while (pos < content.size()) {
        size_t h1;
        if (pos == 0 && content.compare(0, 8, "<<<<<<< ") == 0) h1 = 0;
        else { h1 = content.find("\n<<<<<<< ", pos); if (h1 != std::string::npos) h1++; }
        if (h1 == std::string::npos) break;
        auto h2 = content.find("\n=======\n", h1);
        if (h2 == std::string::npos) break;
        auto h3 = content.find("\n>>>>>>> ", h2);
        if (h3 == std::string::npos) break;
        auto nl1 = content.find('\n', h1);
        if (nl1 == std::string::npos || nl1 >= h2) break;
        std::string ours = content.substr(nl1 + 1, h2 - nl1 - 1);
        size_t nl2 = h2 + 9;
        std::string theirs = content.substr(nl2, h3 - nl2);
        res.push_back({ours, theirs});
        pos = h3 + 1;
    }
}

static void display_conflicts(const std::string &root, const std::vector<std::string> &all_conflicted) {
    (void)root;
    std::vector<std::string> non_lockfile;
    for (auto &cf : all_conflicted) {
        auto s = cf.rfind('/');
        std::string base = (s != std::string::npos) ? cf.substr(s + 1) : cf;
        bool is_lf = false;
        for (auto &[n, _] : lockfiles)
            if (base == n) { is_lf = true; break; }
        if (!is_lf) non_lockfile.push_back(cf);
    }
    if (non_lockfile.empty()) { shown_conflicts.clear(); return; }
    if (non_lockfile == shown_conflicts) return;
    shown_conflicts = non_lockfile;

    int total_hunks = 0;
    for (auto &cf : non_lockfile) {
        FILE *f = fopen(cf.c_str(), "r");
        if (!f) continue;
        std::string content;
        char buf[4096];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), f)) > 0) content.append(buf, n);
        fclose(f);
        std::vector<std::pair<std::string, std::string>> hunks;
        parse_hunks(content, hunks);
        total_hunks += (int)hunks.size();
        for (size_t i = 0; i < hunks.size(); i++) {
            std::cout << "\x1b[1;37m✿ Conflict in " << cf << " (hunk " << (i+1) << " of " << hunks.size() << ")\x1b[0m\n";
            std::cout << "\xe2\x94\x8c\xe2\x94\x80 HEAD (yours) " << std::string(42 - (int)cf.size(), '-') << "\n";
            auto &ours = hunks[i].first;
            size_t ol = 0, onl;
            while ((onl = ours.find('\n', ol)) != std::string::npos) {
                std::cout << "\xe2\x94\x82 " << ours.substr(ol, onl - ol) << '\n';
                ol = onl + 1;
            }
            if (ol < ours.size()) std::cout << "\xe2\x94\x82 " << ours.substr(ol) << '\n';
            std::cout << "\xe2\x94\x9c\xe2\x94\x80 incoming " << std::string(36, '-') << "\n";
            auto &theirs = hunks[i].second;
            size_t tl = 0, tnl;
            while ((tnl = theirs.find('\n', tl)) != std::string::npos) {
                std::cout << "\xe2\x94\x82 " << theirs.substr(tl, tnl - tl) << '\n';
                tl = tnl + 1;
            }
            if (tl < theirs.size()) std::cout << "\xe2\x94\x82 " << theirs.substr(tl) << '\n';
            std::cout << "\xe2\x94\x94" << std::string(53, '-') << "\n";
        }
    }
    std::cout << "✿ " << total_hunks << " conflict(s) in " << non_lockfile.size() << " file(s) — resolve manually, then 'add' and 'commit' as normal.\n";
}

void check_lockfile_conflicts(const std::string &root) {
    FILE *fp = popen("git diff --name-only --diff-filter=U 2>/dev/null", "r");
    if (!fp) return;
    std::vector<std::string> conflicted;
    char line[4096];
    while (fgets(line, sizeof(line), fp)) {
        size_t n = strlen(line);
        while (n > 0 && (line[n-1] == '\n' || line[n-1] == '\r')) line[--n] = '\0';
        if (n > 0) conflicted.push_back(line);
    }
    if (pclose(fp) != 0 || conflicted.empty()) { shown_conflicts.clear(); return; }

    display_conflicts(root, conflicted);

    std::vector<std::string> matched;
    for (auto &cf : conflicted) {
        auto s = cf.rfind('/');
        std::string base = (s != std::string::npos) ? cf.substr(s + 1) : cf;
        for (auto &[n, _] : lockfiles)
            if (base == n) { matched.push_back(cf); break; }
    }
    if (matched.empty()) return;

    std::cout << "✿ Detected " << matched.size() << " lockfile conflict(s):";
    for (auto &m : matched) std::cout << " " << m;
    std::cout << "\n  These can likely be auto-fixed by regenerating instead of" << std::endl;
    std::cout << "  manually merging." << std::endl;
    int other = (int)conflicted.size() - (int)matched.size();

    std::cout << "✿ Auto-regenerate lockfile(s)? [y/N] " << std::flush;
    std::string reply;
    std::getline(std::cin, reply);
    if (reply != "y" && reply != "Y") {
        std::cout << "✿ Skipped — resolve lockfile(s) manually." << std::endl;
        return;
    }

    for (auto &lf : matched) {
        std::string name, cmd;
        auto s = lf.rfind('/');
        std::string base = (s != std::string::npos) ? lf.substr(s + 1) : lf;
        for (auto &[n, c] : lockfiles)
            if (base == n) { name = n; cmd = c; break; }
        if (name.empty()) continue;

        std::string prefix = "cd " + root + " 2>/dev/null && ";
        cmd_ok((prefix + "git checkout --theirs " + lf + " 2>/dev/null").c_str());
        std::string manifest_name;
        for (auto &[ln, mn] : lockfile_manifests)
            if (ln == name) { manifest_name = mn; break; }
        if (!manifest_name.empty()) {
            auto s2 = lf.rfind('/');
            std::string manifest_path = (s2 != std::string::npos) ? lf.substr(0, s2 + 1) + manifest_name : manifest_name;
            cmd_ok((prefix + "git checkout --theirs " + manifest_path + " 2>/dev/null").c_str());
            cmd_ok((prefix + "git add " + manifest_path + " 2>/dev/null").c_str());
        }
        if (!cmd_ok((prefix + cmd + " 2>/dev/null").c_str())) {
            std::cout << "✿ Failed to regenerate " << name << " — resolve manually." << std::endl;
            continue;
        }
        cmd_ok((prefix + "git add " + lf + " 2>/dev/null").c_str());
        std::cout << "✿ " << name << " regenerated and staged." << std::endl;
    }
    if (other > 0)
        std::cout << "✿ Note: " << other << " other conflicted file(s) still need manual resolution." << std::endl;
}

bool is_pull_merge_rebase(const std::string &line) {
    auto tok = split_words(line);
    for (auto &t : tok)
        if (t == "pull" || t == "merge" || t == "rebase") return true;
    return false;
}

bool handle_commit_guard(const std::string &line) {
    std::string cmd = line;
    auto f = cmd.find_first_not_of(" \t");
    if (f != std::string::npos) { auto l = cmd.find_last_not_of(" \t"); cmd = cmd.substr(f, l - f + 1); }
    auto tok = split_words(cmd);
    bool is_commit = (!tok.empty() && tok[0] == "commit") || (tok.size() >= 2 && tok[0] == "git" && tok[1] == "commit");
    if (is_commit) {
        std::string list = run_capture("git diff --name-only --diff-filter=U 2>/dev/null");
        if (!list.empty()) {
            std::string files;
            size_t p = 0, count = 0;
            while (p < list.size() && count < 3) {
                auto nl = list.find('\n', p);
                if (nl != std::string::npos) { if (count > 0) files += ", "; files += list.substr(p, nl - p); p = nl + 1; count++; }
                else { if (count > 0) files += ", "; files += list.substr(p); break; }
            }
            if (count < 3 && p < list.size()) files += ", ...";
            size_t total = 1;
            for (char c : list) if (c == '\n') total++;
            std::cout << "✿ Blocked: " << total << " unresolved conflict(s) remain in " << files << ". Resolve and 'git add' them before committing.\n";
            return true;
        }
    }
    return false;
}
