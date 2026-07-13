# shell integration for whiterose
# source this in your .bashrc / .zshrc:
#   source /path/to/whiterose/integration.sh
#
# Automatically suggests launching whiterose when you cd into a git repo root.
# Only prompts once per repo per session (per PWD).

__whiterose_cd_hook() {
    # only at repo root (where .git exists as dir/file)
    if [ ! -e .git ]; then return; fi

    # don't re-prompt the same repo
    if [ "$WHITEROSE_LAST_REPO" = "$PWD" ]; then return; fi
    export WHITEROSE_LAST_REPO="$PWD"

    echo -n "✿ Enter whiterose? [Y/n] "
    read -r __wr_reply
    if [ "$__wr_reply" != "n" ] && [ "$__wr_reply" != "N" ]; then
        exec whiterose
    fi
    unset __wr_reply
}

if [ -n "$BASH_VERSION" ]; then
    # bash: check cwd after each prompt (handles cd, pushd, popd, z)
    __whiterose_prompt() {
        local rc=$?
        if [ "$PWD" != "${WHITEROSE_LAST_DIR-}" ]; then
            WHITEROSE_LAST_DIR="$PWD"
            __whiterose_cd_hook
        fi
        return $rc
    }
    PROMPT_COMMAND="__whiterose_prompt;${PROMPT_COMMAND}"
elif [ -n "$ZSH_VERSION" ]; then
    # zsh: chpwd hook (fires on dir change)
    chpwd_functions+=(__whiterose_cd_hook)
    # fire once on shell start
    __whiterose_cd_hook 2>/dev/null
fi
