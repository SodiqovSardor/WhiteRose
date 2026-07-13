#include "boot.hpp"
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

std::string find_repo_root() {
    fs::path p = fs::current_path();
    for (;;) {
        if (fs::exists(p / ".git")) return p.string();
        if (p == p.root_path()) break;
        p = p.parent_path();
    }
    return {};
}
