#pragma once
#include <string>

void print_help(bool inside);
std::string make_prompt(int repo_root_len);
void run_repl(const std::string &REPO_ROOT);
