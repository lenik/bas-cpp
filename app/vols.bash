_vols()
{
    local cur prev words cword
    _init_completion -n : || return 0

    # vols currently has no command-line options; do not offer any.
    COMPREPLY=()
    return 0
}

complete -F _vols vols

