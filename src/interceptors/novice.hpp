#pragma once
#include <string>

bool handle_teach_command(const std::string &line);
bool handle_aliases(const std::string &line, std::string &new_line);
void explain_command(const std::string &line);
void normalize_bare_git(std::string &line);
