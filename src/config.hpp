#pragma once
#include <string>
#include <vector>

#define WHITEROSE_VERSION "1.1.0"

struct WhiteRoseConfig {
    std::vector<std::string> protected_branches = {"main", "master"};
    bool auto_upstream = true;
    bool smart_pull = true;
    bool backup_on_destructive = true;
    bool novice_mode = false;
};

extern WhiteRoseConfig cfg;

WhiteRoseConfig load_config(const std::string &root);
