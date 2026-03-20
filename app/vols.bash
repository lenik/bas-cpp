_vols()
{
    local cur prev words cword
    _init_completion -n : || return 0

    local opts='-h --help -a --all -c --compact -m --mountpoint -d --device -t --type -l --label -u --uuid'
    opts+=' -r --read-only -L --logical-type -D --loop-device -o --opts -O --superopts -z --root -s --symbols'
    opts+=' -w --writable -v --verbose -q --quiet'
    if [[ ${cur} == -* ]]; then
        COMPREPLY=($(compgen -W "${opts}" -- "${cur}"))
        return 0
    fi
    COMPREPLY=()
    return 0
}

complete -F _vols vols
