# whiterose completion for zsh
# install: source this file or copy to /usr/share/zsh/site-functions/_whiterose

_whiterose() {
    local -a opts
    opts=(
        '--help[show help]'
        '--version[show version]'
        '-h[show help]'
        '-v[show version]'
    )
    _describe 'whiterose' opts
}
compdef _whiterose whiterose
