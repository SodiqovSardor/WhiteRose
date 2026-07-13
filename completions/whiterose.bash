# whiterose completion for bash
# install: source this file or copy to /etc/bash_completion.d/whiterose

_whiterose() {
    local cur opts
    cur="${COMP_WORDS[COMP_CWORD]}"
    opts="--help --version -h -v"
    COMPREPLY=($(compgen -W "${opts}" -- "${cur}"))
}
complete -F _whiterose whiterose
