#include "push.hpp"
#include "../config.hpp"
#include "../git_utils.hpp"
#include <iostream>
#include <string>

// Returns true if the command was handled (caller should continue the loop)
bool handle_push_interceptors(const std::string &line, std::string &new_line) {
    new_line = line;

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
                else br = get_current_branch();
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
                            new_line = "git " + line.substr(line.find_first_not_of(" \t"));
                    } else {
                        return true; // blocked, skip fork
                    }
                }
            }
        }
    }

    // --- protected branch confirmation ---
    {
        std::string cmd = new_line;
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
            else target = get_current_branch();
            std::string cur = get_current_branch();
            bool is_prot = false;
            for (auto &p : cfg.protected_branches)
                if (target == p && cur == p) { is_prot = true; break; }
            if (is_prot) {
                std::cout << "✿ You're about to push directly to '" << target << "'. Continue? [y/N] " << std::flush;
                std::string reply;
                std::getline(std::cin, reply);
                if (reply != "y" && reply != "Y") { std::cout << "✿ Push cancelled." << std::endl; return true; }
            }
        }
    }

    // --- push auto-upstream intercept ---
    {
        std::string cmd = new_line;
        auto f = cmd.find_first_not_of(" \t");
        if (f != std::string::npos) { auto l = cmd.find_last_not_of(" \t"); cmd = cmd.substr(f, l - f + 1); }
        if (cmd.compare(0, 4, "git ") != 0 && cmd.compare(0, 4, "push") == 0)
            new_line = "git " + new_line.substr(new_line.find_first_not_of(" \t"));
        if (cfg.auto_upstream && (cmd == "push" || cmd == "git push")) {
            std::string br = get_current_branch();
            if (!br.empty() && !cmd_ok("git rev-parse --abbrev-ref --symbolic-full-name @{u} 2>/dev/null")) {
                std::cout << "✿ no upstream set — pushing with --set-upstream origin " << br << std::endl;
                new_line = "git push --set-upstream origin " + br;
            }
        }
    }

    return false; // not handled, proceed to fork
}
