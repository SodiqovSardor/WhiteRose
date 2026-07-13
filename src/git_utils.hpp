#pragma once
#include <string>
#include <vector>

std::vector<std::string> split_words(const std::string &s);
std::string run_capture(const char *cmd);
bool cmd_ok(const char *cmd);
std::string get_current_branch();
