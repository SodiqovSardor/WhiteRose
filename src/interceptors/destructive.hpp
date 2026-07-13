#pragma once
#include <string>
#include <vector>

bool handle_undo_command(const std::string &line);
bool handle_backup(const std::string &line);
bool handle_backups_command(const std::string &line);
bool confirm_destructive(const std::string &line);
bool is_destructive(const std::vector<std::string> &tok);
void save_backup();
void undo();
void list_backups();
