// whiterose v1.1.0 — git-aware shell with destructive-command protection, smart defaults, conflict tooling, and novice mode

#include <iostream>
#include <string>
#include <filesystem>
#include <unistd.h>
#include <sys/wait.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <ctime>
#include <cctype>

static const char *VERSION = "1.1.0";

static std::vector<std::string> split_words(const std::string &s) {
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

static std::string run_capture(const char *cmd) {
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

static bool cmd_ok(const char *cmd) {
    FILE *fp = popen(cmd, "r");
    if (!fp) return false;
    char buf[256];
    while (fgets(buf, sizeof(buf), fp));
    return pclose(fp) == 0;
}

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
            if (sscanf(line, "# branch.head %1023[^\n]", val) == 1)
                branch = val;
            else if (sscanf(line, "# branch.upstream %1023[^\n]", val) == 1)
                upstream = val;
            else if (sscanf(line, "# branch.ab %d %d", &ahead, &behind) >= 1)
                ;
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

// soft limit: keep last 20 backups, delete older on each new backup
static void save_backup() {
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

static void undo() {
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

static bool is_destructive(const std::vector<std::string> &tok) {
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
            if (tok[i] == "--force-with-lease") fl = true;
            if (tok[i] == "--force" || tok[i] == "-f") f = true;
        }
        return f && !fl;
    }
    if (c == "rebase") return true;
    return false;
}

struct WhiteRoseConfig {
    std::vector<std::string> protected_branches = {"main", "master"};
    bool auto_upstream = true;
    bool smart_pull = true;
    bool backup_on_destructive = true;
    bool novice_mode = false;
};

static WhiteRoseConfig load_config(const std::string &root) {
    WhiteRoseConfig cfg;
    std::string path = root + "/.whiterose.toml";
    FILE *fp = fopen(path.c_str(), "r");
    if (!fp) {
        // auto-create with defaults
        fp = fopen(path.c_str(), "w");
        if (fp) {
            fprintf(fp, "# whiterose config — see README for all options\n");
            fprintf(fp, "protected_branches = [\"main\", \"master\"]\n");
            fprintf(fp, "auto_upstream = true\n");
            fprintf(fp, "smart_pull = true\n");
            fprintf(fp, "backup_on_destructive = true\n");
            fprintf(fp, "novice_mode = false\n");
            fclose(fp);
            std::cout << "✿ created .whiterose.toml with defaults\n";
        }
        return cfg;
    }
    char line[1024];
    int lineno = 0;
    while (fgets(line, sizeof(line), fp)) {
        lineno++;
        size_t n = strlen(line);
        while (n > 0 && (line[n-1] == '\n' || line[n-1] == '\r' || line[n-1] == ' ' || line[n-1] == '\t')) line[--n] = '\0';
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0' || *p == '#') continue;
        char *eq = strchr(p, '=');
        if (!eq) { std::cerr << "✿ warning: could not parse line " << lineno << " in .whiterose.toml\n"; continue; }
        *eq = '\0';
        char *ke = eq - 1;
        while (ke >= p && (*ke == ' ' || *ke == '\t')) *ke-- = '\0';
        char *val = eq + 1;
        while (*val == ' ' || *val == '\t') val++;
        std::string k(p), v(val);
        auto to_lower = [](std::string s) { for (auto &c : s) c = tolower(c); return s; };
        if (k == "protected_branches") {
            auto s = v.find('['), e = v.find(']');
            if (s != std::string::npos && e != std::string::npos && e > s) {
                cfg.protected_branches.clear();
                std::string inner = v.substr(s + 1, e - s - 1);
                size_t i = 0;
                while (i < inner.size()) {
                    while (i < inner.size() && (inner[i] == ' ' || inner[i] == '\t' || inner[i] == ',')) i++;
                    if (i >= inner.size()) break;
                    if (inner[i] == '"') {
                        i++;
                        size_t q = inner.find('"', i);
                        if (q != std::string::npos) { cfg.protected_branches.push_back(inner.substr(i, q - i)); i = q + 1; }
                    }
                }
            } else { std::cerr << "✿ warning: could not parse line " << lineno << " in .whiterose.toml\n"; }
        } else if (k == "auto_upstream") {
            auto lv = to_lower(v);
            if (lv == "true") cfg.auto_upstream = true;
            else if (lv == "false") cfg.auto_upstream = false;
            else { std::cerr << "✿ warning: could not parse line " << lineno << " in .whiterose.toml\n"; }
        } else if (k == "smart_pull") {
            auto lv = to_lower(v);
            if (lv == "true") cfg.smart_pull = true;
            else if (lv == "false") cfg.smart_pull = false;
            else { std::cerr << "✿ warning: could not parse line " << lineno << " in .whiterose.toml\n"; }
        } else if (k == "backup_on_destructive") {
            auto lv = to_lower(v);
            if (lv == "true") cfg.backup_on_destructive = true;
            else if (lv == "false") cfg.backup_on_destructive = false;
            else { std::cerr << "✿ warning: could not parse line " << lineno << " in .whiterose.toml\n"; }
        } else if (k == "novice_mode") {
            auto lv = to_lower(v);
            if (lv == "true") cfg.novice_mode = true;
            else if (lv == "false") cfg.novice_mode = false;
            else { std::cerr << "✿ warning: could not parse line " << lineno << " in .whiterose.toml\n"; }
        } else {
            std::cerr << "✿ warning: could not parse line " << lineno << " in .whiterose.toml\n";
        }
    }
    fclose(fp);
    std::cout << "✿ loaded .whiterose.toml (" << cfg.protected_branches.size() << " protected branches)" << std::endl;
    return cfg;
}

static WhiteRoseConfig cfg;

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

// track which (non-lockfile) conflicted files we've already shown
static std::vector<std::string> shown_conflicts;

// parse conflict markers from file content into (ours, theirs) hunks
// future: per-hunk blame attribution (out of scope for v0.0.10)
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
        size_t nl2 = h2 + 9; // length of "\n=======\n"
        std::string theirs = content.substr(nl2, h3 - nl2);
        res.push_back({ours, theirs});
        pos = h3 + 1;
    }
}

// display boxed conflict view for non-lockfile conflicted files
// tracks shown files to avoid repeating on every piped command
static void display_conflicts(const std::string &root, const std::vector<std::string> &all_conflicted) {
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

    // same set as last time? skip
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

static void check_lockfile_conflicts(const std::string &root) {
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

    // display non-lockfile conflicts
    display_conflicts(root, conflicted);

    // lockfile auto-fix (v0.0.9)
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

static std::string make_prompt() {
    std::string dir = fs::current_path().filename().string();
    std::string br = git_branch();
    return "\001\x1b[1;37m\002✿ \001\x1b[1;34m\002" + dir
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

// tab completion: branch names for checkout/switch/merge/rebase/branch
static char *branch_gen(const char *text, int state) {
    static std::vector<std::string> matches;
    static size_t idx;
    if (state == 0) {
        matches.clear(); idx = 0;
        std::string linebuf(rl_line_buffer);
        auto tok = split_words(linebuf);
        if (tok.size() < 2) return nullptr;
        // first token must be a branch-command or "git" + branch-command
        size_t ci = (tok[0] == "git" && tok.size() >= 3) ? 1 : 0;
        const std::string &c = tok[ci];
        if (!(c == "checkout" || c == "switch" || c == "merge" ||
              c == "rebase" || c == "branch"))
            return nullptr;
        if (c == "checkout") {
            for (size_t i = ci + 1; i < tok.size(); i++)
                if (tok[i] == "-b") return nullptr;
        }
        if (text[0] == '-') return nullptr;

        // also include remote branches, stripping origin/ prefix
        FILE *fp = popen("git branch -a --format='%(refname:short)' 2>/dev/null", "r");
        if (!fp) return nullptr;
        char buf[4096];
        size_t tlen = strlen(text);
        while (fgets(buf, sizeof(buf), fp)) {
            size_t n = strlen(buf);
            while (n > 0 && (buf[n-1] == '\n' || buf[n-1] == '\r')) buf[--n] = '\0';
            if (n == 0) continue;
            std::string name(buf);
            // strip remote prefix
            auto slash = name.rfind('/');
            if (name.compare(0, 8, "remotes/") == 0 && slash != std::string::npos)
                name = name.substr(slash + 1);
            // skip HEAD ref
            if (name == "HEAD") continue;
            if (tlen == 0 || name.compare(0, tlen, text) == 0)
                matches.push_back(name);
        }
        pclose(fp);
    }
    if (idx < matches.size()) return strdup(matches[idx++].c_str());
    return nullptr;
}

static void print_help(bool inside) {
    std::cout << "whiterose v" << VERSION << " — a git-aware shell that protects you from\n"
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
        "  undo              restores your most recent backup\n"
        "  commit            blocked if unresolved merge conflicts remain\n\n"
        "Beginner aliases (always on):\n"
        "  new-branch <name>  creates a new branch and switches to it\n"
        "  switch <name>      switches to an existing branch\n"
        "  save               stages all changes and commits them\n"
        "  sync               pull latest changes from remote\n"
        "  share              push your changes to the remote\n"
        "  teach on/off       toggle plain-language explanations\n\n"
        "Config: place a .whiterose.toml in your repo root to customize\n"
        "protected branches and toggle features. See README for format.\n\n"
        "Exit the shell any time with 'exit' or Ctrl-D.\n";
}

int main(int argc, char **argv) {
    if (argc > 1) {
        std::string flag(argv[1]);
        if (flag == "--version" || flag == "-v") { std::cout << "whiterose v" << VERSION << std::endl; return 0; }
        if (flag == "--help" || flag == "-h") { print_help(false); return 0; }
    }
    std::string REPO_ROOT = find_repo_root();
    if (REPO_ROOT.empty()) {
        std::cout << "✿ White Rose: Crimson error. Not a git repository.\n";
        return 1;
    }
    std::cout << "✿ whiterose v" << VERSION << " booted at " << REPO_ROOT << std::endl;
    cfg = load_config(REPO_ROOT);

    rl_completion_entry_function = branch_gen;
    rl_completion_append_character = '\0';
    rl_variable_bind("show-all-if-ambiguous", "on");

    char *input;
    while ((input = readline(make_prompt().c_str())) != nullptr) {
        std::string line(input);
        free(input);

        if (line == "exit" || line == "quit") {
            std::cout << "bye\n";
            break;
        }
        if (line == "help") { print_help(true); continue; }

        if (!line.empty()) add_history(line.c_str());

        // --- teach on/off toggle ---
        {
            std::string cmd = line;
            auto f = cmd.find_first_not_of(" \t");
            if (f != std::string::npos) { auto l = cmd.find_last_not_of(" \t"); cmd = cmd.substr(f, l - f + 1); }
            if (cmd == "teach on") { cfg.novice_mode = true; std::cout << "✿ Novice mode ON — I'll explain what commands do before running them.\n"; continue; }
            if (cmd == "teach off") { cfg.novice_mode = false; std::cout << "✿ Novice mode OFF.\n"; continue; }
        }
        // --- end teach toggle ---

        // --- beginner aliases (always on) ---
        {
            std::string cmd = line;
            auto f = cmd.find_first_not_of(" \t");
            if (f != std::string::npos) { auto l = cmd.find_last_not_of(" \t"); cmd = cmd.substr(f, l - f + 1); }
            auto tok = split_words(cmd);
            if (!tok.empty()) {
                if (tok[0] == "new-branch" && tok.size() >= 2) {
                    std::cout << "✿ (running: git checkout -b " << tok[1] << ")\n";
                    line = "git checkout -b " + tok[1];
                } else if (tok[0] == "switch" && tok.size() >= 2) {
                    std::cout << "✿ (running: git checkout " << tok[1] << ")\n";
                    line = "git checkout " + tok[1];
                } else if (tok[0] == "save") {
                    std::cout << "✿ Commit message: " << std::flush;
                    std::string msg;
                    std::getline(std::cin, msg);
                    if (msg.empty()) { std::cout << "✿ Commit cancelled — message required.\n"; continue; }
                    for (size_t i = 0; i < msg.size(); i++)
                        if (msg[i] == '"' || msg[i] == '\\') { msg.insert(i, 1, '\\'); i++; }
                    std::cout << "✿ (running: git add -A && git commit -m \"" << msg << "\")\n";
                    line = "git add -A && git commit -m \"" + msg + "\"";
                } else if (tok[0] == "sync") {
                    std::cout << "✿ (running: git pull)\n";
                    line = "git pull";
                } else if (tok[0] == "share") {
                    std::cout << "✿ (running: git push)\n";
                    line = "git push";
                }
            }
        }
        // --- end aliases ---

        // --- novice mode explanations ---
        if (cfg.novice_mode) {
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
        // --- end novice mode ---

        // --- status reformat intercept ---
        {
            std::string cmd = line;
            auto f = cmd.find_first_not_of(" \t");
            if (f != std::string::npos) { auto l = cmd.find_last_not_of(" \t"); cmd = cmd.substr(f, l - f + 1); }
            if (cmd == "status" || cmd == "st" || cmd == "git status") { parseAndPrintStatus(); continue; }
        }
        // --- end status intercept ---

        // --- force-push protection ---
        {
            std::string cmd = line;
            auto f = cmd.find_first_not_of(" \t");
            if (f != std::string::npos) { auto l = cmd.find_last_not_of(" \t"); cmd = cmd.substr(f, l - f + 1); }
            auto tok = split_words(cmd);
            size_t pi = (tok.size() >= 2 && tok[0] == "git" && tok[1] == "push") ? 1
                      : (!tok.empty() && tok[0] == "push") ? 0 : (size_t)-1;
            if (pi != (size_t)-1) {
                bool has_f = false, has_fwl = false;
                std::vector<std::string> rest;
                for (size_t i = pi + 1; i < tok.size(); i++) {
                    if (tok[i] == "--force-with-lease") has_fwl = true;
                    else if (tok[i] == "--force" || tok[i] == "-f") has_f = true;
                    else if (tok[i][0] != '-') rest.push_back(tok[i]);
                }
                if (has_f && !has_fwl) {
                    std::string br;
                    if (rest.size() >= 2) br = rest.back();
                    else br = run_capture("git branch --show-current 2>/dev/null");
                    if (!br.empty()) {
                        bool proceed = true;
                        std::string fetch = "git fetch origin " + br + " --quiet 2>/dev/null";
                        if (!cmd_ok(fetch.c_str())) {
                            std::cout << "✿ Could not verify remote state — proceed with caution" << std::endl;
                        } else {
                            std::string check = "git rev-list --count " + br + "..origin/" + br + " 2>/dev/null";
                            std::string cnt = run_capture(check.c_str());
                            if (!cnt.empty()) {
                                int n = std::stoi(cnt);
                                if (n > 0) {
                                    std::cout << "✿ Blocked: remote has " << n << " commit(s) you don't have locally." << std::endl;
                                    std::cout << "   Force-pushing would destroy them. Use 'push --force-with-lease'" << std::endl;
                                    std::cout << "   if you're sure, or pull first." << std::endl;
                                    proceed = false;
                                }
                            }
                        }
                        if (proceed) {
                            if (cmd.compare(0, 4, "git ") != 0)
                                line = "git " + line.substr(line.find_first_not_of(" \t"));
                        } else { continue; }
                    }
                }
            }
        }
        // --- end force-push protection ---

        // --- protected branch confirmation ---
        {
            std::string cmd = line;
            auto f = cmd.find_first_not_of(" \t");
            if (f != std::string::npos) { auto l = cmd.find_last_not_of(" \t"); cmd = cmd.substr(f, l - f + 1); }
            auto tok = split_words(cmd);
            size_t pi = (tok.size() >= 2 && tok[0] == "git" && tok[1] == "push") ? 1
                      : (!tok.empty() && tok[0] == "push") ? 0 : (size_t)-1;
            if (pi != (size_t)-1) {
                std::vector<std::string> rest;
                for (size_t i = pi + 1; i < tok.size(); i++)
                    if (tok[i][0] != '-') rest.push_back(tok[i]);
                std::string target;
                if (rest.size() >= 2) target = rest.back();
                else target = run_capture("git branch --show-current 2>/dev/null");
                std::string cur = run_capture("git branch --show-current 2>/dev/null");
                bool is_prot = false;
                for (auto &p : cfg.protected_branches)
                    if (target == p && cur == p) { is_prot = true; break; }
                if (is_prot) {
                    std::cout << "✿ You're about to push directly to '" << target << "'. Continue? [y/N] " << std::flush;
                    std::string reply;
                    std::getline(std::cin, reply);
                    if (reply != "y" && reply != "Y") { std::cout << "✿ Push cancelled." << std::endl; continue; }
                }
            }
        }
        // --- end protected branch ---

        // --- push auto-upstream intercept ---
        {
            std::string cmd = line;
            auto f = cmd.find_first_not_of(" \t");
            if (f != std::string::npos) { auto l = cmd.find_last_not_of(" \t"); cmd = cmd.substr(f, l - f + 1); }
            if (cmd.compare(0, 4, "git ") != 0 && cmd.compare(0, 4, "push") == 0)
                line = "git " + line.substr(line.find_first_not_of(" \t"));
            if (cfg.auto_upstream && (cmd == "push" || cmd == "git push")) {
                std::string br = run_capture("git branch --show-current 2>/dev/null");
                if (!br.empty() && !cmd_ok("git rev-parse --abbrev-ref --symbolic-full-name @{u} 2>/dev/null")) {
                    std::cout << "✿ no upstream set — pushing with --set-upstream origin " << br << std::endl;
                    line = "git push --set-upstream origin " + br;
                }
            }
        }
        // --- end push intercept ---

        // --- smart pull (rebase-if-safe) ---
        {
            std::string cmd = line;
            auto f = cmd.find_first_not_of(" \t");
            if (f != std::string::npos) { auto l = cmd.find_last_not_of(" \t"); cmd = cmd.substr(f, l - f + 1); }
            if (cmd.compare(0, 4, "git ") != 0 && cmd.compare(0, 4, "pull") == 0)
                line = "git " + line.substr(line.find_first_not_of(" \t"));
            if (cfg.smart_pull && (cmd == "pull" || cmd == "git pull")) {
                std::string up = run_capture("git rev-parse --abbrev-ref --symbolic-full-name @{u} 2>/dev/null");
                if (!up.empty()) {
                    std::string dirty = run_capture("git status --porcelain 2>/dev/null");
                    if (!dirty.empty()) {
                        std::cout << "✿ Uncommitted changes present — using regular pull (merge)\n   to avoid rebase conflicts. Commit or stash first\n   if you want a clean rebase.\n";
                        line = "git pull";
                    } else {
                        std::cout << "✿ pulling with --rebase (clean working tree)\n";
                        line = "git pull --rebase";
                    }
                }
            }
        }
        // --- end smart pull ---

        // --- undo (before backup — never forks) ---
        {
            std::string cmd = line;
            auto f = cmd.find_first_not_of(" \t");
            if (f != std::string::npos) { auto l = cmd.find_last_not_of(" \t"); cmd = cmd.substr(f, l - f + 1); }
            if (cmd == "undo") { undo(); continue; }
        }
        // --- end undo ---

        // --- commit guard (block if unresolved conflicts remain) ---
        {
            std::string cmd = line;
            auto f = cmd.find_first_not_of(" \t");
            if (f != std::string::npos) { auto l = cmd.find_last_not_of(" \t"); cmd = cmd.substr(f, l - f + 1); }
            auto tok = split_words(cmd);
            bool is_commit = (!tok.empty() && tok[0] == "commit") || (tok.size() >= 2 && tok[0] == "git" && tok[1] == "commit");
            if (is_commit) {
                std::string list = run_capture("git diff --name-only --diff-filter=U 2>/dev/null");
                if (!list.empty()) {
                    // read the first few file names for the message
                    std::string files;
                    size_t p = 0, count = 0;
                    while (p < list.size() && count < 3) {
                        auto nl = list.find('\n', p);
                        if (nl != std::string::npos) { if (count > 0) files += ", "; files += list.substr(p, nl - p); p = nl + 1; count++; }
                        else { if (count > 0) files += ", "; files += list.substr(p); break; }
                    }
                    if (count < 3 && p < list.size()) files += ", ...";
                    // Count total
                    size_t total = 1;
                    for (char c : list) if (c == '\n') total++;
                    std::cout << "✿ Blocked: " << total << " unresolved conflict(s) remain in " << files << ". Resolve and 'git add' them before committing.\n";
                    continue;
                }
            }
        }
        // --- end commit guard ---

        // --- backup before destructive commands ---
        {
            auto tok = split_words(line);
            if (cfg.backup_on_destructive && is_destructive(tok))
                save_backup();
        }
        // --- end backup ---

        int pfd[2];
        pipe(pfd);

        pid_t pid = fork();
        if (pid < 0) {
            std::cerr << "✿ fork failed — skipping command\n";
            close(pfd[0]); close(pfd[1]);
            continue;
        }
        if (pid == 0) {
            close(pfd[0]);
            dup2(pfd[1], 3);
            close(pfd[1]);
            execlp("/bin/bash", "bash", "-i", "-c",
                (line + "; echo \"$PWD\" >&3 2>/dev/null").c_str(), nullptr);
            _exit(127);
        }
        close(pfd[1]);

        waitpid(pid, nullptr, 0);

        char buf[4096];
        ssize_t n = read(pfd[0], buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            std::string d(buf);
            while (!d.empty() && (d.back() == '\n' || d.back() == '\r')) d.pop_back();
            if (!d.empty()) chdir(d.c_str());
        }
        close(pfd[0]);

        // --- conflict display + lockfile check ---
        {
            auto tok = split_words(line);
            bool check = false;
            for (auto &t : tok)
                if (t == "pull" || t == "merge" || t == "rebase") { check = true; break; }
            if (check) check_lockfile_conflicts(REPO_ROOT);
        }
        // --- end conflict check ---
    }
}
// g++ -std=c++17 -Os -s main.cpp -o whiterose -lreadline
