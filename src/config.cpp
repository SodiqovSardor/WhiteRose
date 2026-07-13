#include "config.hpp"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <cctype>

WhiteRoseConfig cfg;

static std::string to_lower(std::string s) {
    for (auto &c : s) c = tolower(c);
    return s;
}

WhiteRoseConfig load_config(const std::string &root, bool quiet) {
    WhiteRoseConfig cfg;
    std::string path = root + "/.whiterose.toml";
    FILE *fp = fopen(path.c_str(), "r");
    if (!fp) {
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
    if (!quiet)
        std::cout << "✿ loaded .whiterose.toml (" << cfg.protected_branches.size() << " protected branches)" << std::endl;
    return cfg;
}

bool handle_config_command(const std::string &line, const std::string &repo_root) {
    std::string cmd = line;
    auto f = cmd.find_first_not_of(" \t");
    if (f != std::string::npos) { auto l = cmd.find_last_not_of(" \t"); cmd = cmd.substr(f, l - f + 1); }
    if (cmd == "config reload") {
        cfg = load_config(repo_root, true);
        return true;
    }
    return false;
}
