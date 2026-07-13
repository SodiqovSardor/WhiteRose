# shell integration for whiterose
# source this in your .bashrc / .zshrc:
#   source /path/to/whiterose/integration.sh
#
# Prompts to enter whiterose every time you cd into a git repo root.
# Say Y or press Enter → whiterose starts. Say n → stays in shell.
# Exiting whiterose returns to your shell.

__whiterose_cd_hook() {
    if [ ! -e .git ]; then return; fi

    echo -n "✿ Enter whiterose? [Y/n] "
    read -r __wr_reply
    if [ "$__wr_reply" != "n" ] && [ "$__wr_reply" != "N" ]; then
        whiterose
    fi
    unset __wr_reply
}

if [ -n "$BASH_VERSION" ]; then
    cd() {
        builtin cd "$@" || return
        __whiterose_cd_hook
    }
elif [ -n "$ZSH_VERSION" ]; then
    chpwd_functions+=(__whiterose_cd_hook)
    __whiterose_cd_hook 2>/dev/null
fi
