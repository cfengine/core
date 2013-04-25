# CFEngine bash completion  -*- shell-script -*-

_cfengine()
{
    local cur prev words cword split
    _init_completion -s || return

    $split && return

    if [[ $cur == -* ]]; then
        COMPREPLY=( $( compgen -W '$( _parse_help "$1" --help )' \
            -- "$cur" ) )
        [[ $COMPREPLY == *= ]] && compopt -o nospace
        return 0
    fi

    _expand || return 0

    compopt -o filenames
    COMPREPLY=( $( compgen -f -X "!*.@(cf|json)" -- "$cur" ) \
        $( compgen -d -- "$cur" ) )
} &&
complete -F _cfengine cf-promises cf-agent cf-execd cf-serverd cf-runagent cf-monitord
