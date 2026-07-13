# shell integration for whiterose
# source this in your .bashrc / .zshrc:
#   source /path/to/whiterose/integration.sh
#
# Automatically launches whiterose when you cd into a git repo root.
# Prompts once per repo per session (per PWD).

__whiterose_cd_hook() {
    if [ ! -e .git ]; then return; fi
    if [ "$WHITEROSE_LAST_REPO" = "$PWD" ]; then return; fi
    export WHITEROSE_LAST_REPO="$PWD"

    echo -n "✿ Enter whiterose? [Y/n] "
    read -r __wr_reply
    if [ "$__wr_reply" != "n" ] && [ "$__wr_reply" != "N" ]; then
        whiterose
    fi
    unset __wr_reply
}

if [ -n "$BASH_VERSION" ]; then
    # override cd — fires only on explicit cd, read works reliably
    cd() {
        builtin cd "$@" || return
        __whiterose_cd_hook
    }
elif [ -n "$ZSH_VERSION" ]; then
    chpwd_functions+=(__whiterose_cd_hook)
    __whiterose_cd_hook 2>/dev/null
fi
