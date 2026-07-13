#include "completion.hpp"
#include "git_utils.hpp"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <readline/readline.h>

static char *branch_gen(const char *text, int state) {
    static std::vector<std::string> matches;
    static size_t idx;
    if (state == 0) {
        matches.clear(); idx = 0;
        std::string linebuf(rl_line_buffer);
        auto tok = split_words(linebuf);
        if (tok.size() < 2) return nullptr;
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

        FILE *fp = popen("git branch -a --format='%(refname:short)' 2>/dev/null", "r");
        if (!fp) return nullptr;
        char buf[4096];
        size_t tlen = strlen(text);
        while (fgets(buf, sizeof(buf), fp)) {
            size_t n = strlen(buf);
            while (n > 0 && (buf[n-1] == '\n' || buf[n-1] == '\r')) buf[--n] = '\0';
            if (n == 0) continue;
            std::string name(buf);
            auto slash = name.rfind('/');
            if (name.compare(0, 8, "remotes/") == 0 && slash != std::string::npos)
                name = name.substr(slash + 1);
            if (name == "HEAD") continue;
            if (tlen == 0 || name.compare(0, tlen, text) == 0)
                matches.push_back(name);
        }
        pclose(fp);
    }
    if (idx < matches.size()) return strdup(matches[idx++].c_str());
    return nullptr;
}

void setup_completion() {
    rl_completion_entry_function = branch_gen;
    rl_completion_append_character = '\0';
    rl_variable_bind("show-all-if-ambiguous", "on");
}
