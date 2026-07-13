#include "destructive.hpp"
#include "../config.hpp"
#include "../git_utils.hpp"
#include <ctime>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

bool is_destructive(const std::vector<std::string> &tok) {
    size_t s = 0;
    if (tok.size() >= 2 && tok[0] == "git") s = 1;
    if (s >= tok.size()) return false;
    const std::string &c = tok[s];
    if (c == "reset") {
        for (size_t i = s + 1; i < tok.size(); i++)
            if (tok[i] == "--hard") return true;
        return false;
    }
    if (c == "checkout") {
        for (size_t i = s + 1; i < tok.size(); i++)
            if (tok[i] == "--") return true;
        return false;
    }
    if (c == "clean") {
        for (size_t i = s + 1; i < tok.size(); i++) {
            const std::string &f = tok[i];
            if (f.size() >= 2 && f[0] == '-' && f[1] == 'f') return true;
        }
        return false;
    }
    if (c == "push") {
        bool f = false, fl = false;
        for (size_t i = s + 1; i < tok.size(); i++) {
            if (tok[i].compare(0, 20, "--force-with-lease") == 0) fl = true;
            if (tok[i] == "--force" || tok[i] == "-f") f = true;
        }
        return f && !fl;
    }
    if (c == "rebase") return true;
    if (c == "branch") {
        for (size_t i = s + 1; i < tok.size(); i++)
            if (tok[i] == "-D" || tok[i] == "-d" || tok[i] == "--delete") return true;
        return false;
    }
    if (c == "stash") {
        for (size_t i = s + 1; i < tok.size(); i++)
            if (tok[i] == "drop" || tok[i] == "clear") return true;
        return false;
    }
    if (c == "rm") return true;
    if (c == "tag") {
        for (size_t i = s + 1; i < tok.size(); i++)
            if (tok[i] == "-d" || tok[i] == "--delete") return true;
        return false;
    }
    if (c == "commit") {
        for (size_t i = s + 1; i < tok.size(); i++)
            if (tok[i] == "--amend") return true;
        return false;
    }
    return false;
}

bool confirm_destructive(const std::string &line) {
    auto tok = split_words(line);
    if (!is_destructive(tok)) return false;
    size_t s = 0;
    if (tok.size() >= 2 && tok[0] == "git") s = 1;
    if (s >= tok.size()) return false;
    const std::string &c = tok[s];

    if (c == "branch") {
        for (size_t i = s + 1; i < tok.size(); i++) {
            if (tok[i] == "-d" || tok[i] == "-D" || tok[i] == "--delete") {
                std::string br = (i + 1 < tok.size()) ? tok[i + 1] : "";
                std::cout << "✿ This will delete branch " << br << ". Continue? [y/N] " << std::flush;
                break;
            }
        }
    } else if (c == "stash") {
        std::cout << "✿ This will permanently remove stash entries. Continue? [y/N] " << std::flush;
    } else if (c == "rm") {
        std::cout << "✿ This will remove file(s) from disk. Continue? [y/N] " << std::flush;
    } else if (c == "tag") {
        for (size_t i = s + 1; i < tok.size(); i++) {
            if (tok[i] == "-d" || tok[i] == "--delete") {
                std::string tg = (i + 1 < tok.size()) ? tok[i + 1] : "";
                std::cout << "✿ This will delete tag " << tg << ". Continue? [y/N] " << std::flush;
                break;
            }
        }
    } else if (c == "commit") {
        std::cout << "✿ This will rewrite the most recent commit. Continue? [y/N] " << std::flush;
    } else {
        return false; // known-bad path handled by push.cpp etc.
    }

    std::string reply;
    std::getline(std::cin, reply);
    if (reply != "y" && reply != "Y") {
        std::cout << "✿ Cancelled.\n";
        return true; // handled / skip fork
    }
    return false;
}

void save_backup() {
    std::string head = run_capture("git rev-parse HEAD 2>/dev/null");
    if (head.empty()) {
        std::cerr << "✿ Could not create backup — proceeding without safety net\n";
        return;
    }
    time_t now = time(nullptr);
    struct tm *tm = localtime(&now);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y%m%d-%H%M%S", tm);
    std::string timestamp(ts);
    std::string rcmd = "git update-ref refs/whiterose/backup-" + timestamp + " " + head + " 2>/dev/null";
    if (!cmd_ok(rcmd.c_str())) {
        std::cerr << "✿ Could not create backup — proceeding without safety net\n";
        return;
    }
    if (cmd_ok("git stash push -u -m 'whiterose-backup' 2>/dev/null")) {
        std::string stash = run_capture("git rev-parse stash@{0} 2>/dev/null");
        if (!stash.empty()) {
            cmd_ok("git stash apply stash@{0} 2>/dev/null");
            cmd_ok("git stash drop -q 2>/dev/null");
            cmd_ok(("git update-ref refs/whiterose/backup-" + timestamp + "-wip " + stash + " 2>/dev/null").c_str());
        }
    }
    {
        FILE *fp = popen("git for-each-ref --sort=refname --format='%(refname)' refs/whiterose/backup-* 2>/dev/null | grep -v -- -wip$", "r");
        if (fp) {
            std::vector<std::string> refs;
            char ln[4096];
            while (fgets(ln, sizeof(ln), fp)) {
                size_t n = strlen(ln);
                while (n > 0 && (ln[n-1] == '\n' || ln[n-1] == '\r')) ln[--n] = '\0';
                if (n > 0) refs.push_back(ln);
            }
            pclose(fp);
            if (refs.size() > 20) {
                for (size_t i = 0; i < refs.size() - 20; i++) {
                    cmd_ok(("git update-ref -d " + refs[i] + " 2>/dev/null").c_str());
                    cmd_ok(("git update-ref -d " + refs[i] + "-wip 2>/dev/null").c_str());
                }
            }
        }
    }
    std::cout << "✿ backup saved: " << timestamp << std::endl;
}

void list_backups() {
    FILE *fp = popen("git for-each-ref --sort=-refname --format='%(refname:short)' refs/whiterose/backup-* 2>/dev/null | grep -v -- -wip$", "r");
    if (!fp) { std::cout << "✿ No backups found.\n"; return; }
    bool any = false;
    char line[4096];
    while (fgets(line, sizeof(line), fp)) {
        size_t n = strlen(line);
        while (n > 0 && (line[n-1] == '\n' || line[n-1] == '\r')) line[--n] = '\0';
        if (n == 0) continue;
        std::string sha = run_capture(("git rev-parse " + std::string(line) + " 2>/dev/null").c_str());
        if (!any) { std::cout << "✿ Backups:\n"; any = true; }
        auto bp = std::string(line).find("backup-");
        std::string ts = (bp != std::string::npos) ? std::string(line).substr(bp + 7) : std::string(line);
        std::cout << "  " << ts << "  " << sha.substr(0, 7) << std::endl;
    }
    pclose(fp);
    if (!any) std::cout << "✿ No backups found.\n";
}

bool handle_backups_command(const std::string &line) {
    std::string cmd = line;
    auto f = cmd.find_first_not_of(" \t");
    if (f != std::string::npos) { auto l = cmd.find_last_not_of(" \t"); cmd = cmd.substr(f, l - f + 1); }
    if (cmd == "backups") { list_backups(); return true; }
    return false;
}

void undo() {
    FILE *fp = popen("git for-each-ref --sort=-refname --format='%(refname)' refs/whiterose/backup-* 2>/dev/null | grep -v -- -wip$", "r");
    if (!fp) { std::cout << "✿ Nothing to undo — no backups found.\n"; return; }
    std::string ref;
    char line[4096];
    while (fgets(line, sizeof(line), fp)) {
        size_t n = strlen(line);
        while (n > 0 && (line[n-1] == '\n' || line[n-1] == '\r')) line[--n] = '\0';
        if (n > 0) { ref = line; break; }
    }
    pclose(fp);
    if (ref.empty()) { std::cout << "✿ Nothing to undo — no backups found.\n"; return; }
    std::string sha = run_capture(("git rev-parse " + ref + " 2>/dev/null").c_str());
    if (sha.empty()) { std::cout << "✿ Error: backup ref exists but could not be read.\n"; return; }
    auto ts_start = ref.find("backup-");
    std::string timestamp = (ts_start != std::string::npos) ? ref.substr(ts_start + 7) : "";
    std::cout << "✿ Restoring to backup " << timestamp << " (commit " << sha.substr(0, 7) << ")" << std::endl;
    if (!cmd_ok(("git reset --hard " + sha + " 2>/dev/null").c_str())) {
        std::cout << "✿ Error: reset failed. Backup ref preserved.\n";
        return;
    }
    std::string wip_ref = "refs/whiterose/backup-" + timestamp + "-wip";
    std::string wip_sha = run_capture(("git rev-parse " + wip_ref + " 2>/dev/null").c_str());
    if (!wip_sha.empty())
        cmd_ok(("git stash apply " + wip_sha + " 2>/dev/null").c_str());
    cmd_ok(("git update-ref -d " + ref + " 2>/dev/null").c_str());
    if (!wip_sha.empty())
        cmd_ok(("git update-ref -d " + wip_ref + " 2>/dev/null").c_str());
    std::cout << "✿ Restored." << std::endl;
}

bool handle_undo_command(const std::string &line) {
    std::string cmd = line;
    auto f = cmd.find_first_not_of(" \t");
    if (f != std::string::npos) { auto l = cmd.find_last_not_of(" \t"); cmd = cmd.substr(f, l - f + 1); }
    if (cmd == "undo") { undo(); return true; }
    return false;
}

bool handle_backup(const std::string &line) {
    auto tok = split_words(line);
    if (cfg.backup_on_destructive && is_destructive(tok)) {
        save_backup();
    }
    return false;
}
