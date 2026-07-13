#pragma once
#include <string>

bool handle_commit_guard(const std::string &line);
void check_lockfile_conflicts(const std::string &root);
bool is_pull_merge_rebase(const std::string &line);
