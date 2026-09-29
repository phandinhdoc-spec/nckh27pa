#!/usr/bin/env bash
# GPIO25 gesture probe. Prints SINGLE/DOUBLE/TRIPLE; never changes device mode.
# Use --stdin for a synthetic event stream, otherwise gpiomon owns GPIO25.
set -uo pipefail

window=1.5
source_mode=${1:---gpio}
if [[ $source_mode != --gpio && $source_mode != --stdin ]]; then
    printf 'Usage: %s [--gpio|--stdin]\n' "$0" >&2
    exit 2
fi

classify() {
    case $1 in
        1) printf 'SINGLE\n' ;;
        2) printf 'DOUBLE\n' ;;
        3) printf 'TRIPLE\n' ;;
        *) printf 'RESERVED(%s)\n' "$1" ;;
    esac
}

# A separate file descriptor prevents EOF on stdin from being mistaken for a
# timeout. In GPIO mode, gpiomon blocks in the kernel until a falling edge.
if [[ $source_mode == --gpio ]]; then
    command -v gpiomon >/dev/null || { printf 'gpiomon is unavailable\n' >&2; exit 1; }
    exec 3< <(gpiomon --chip gpiochip0 --edges falling --bias pull-up \
        --debounce-period 60ms --format '%E' 25)
else
    exec 3<&0
fi

count=0
while :; do
    event=''
    if (( count == 0 )); then
        if ! IFS= read -r event <&3; then
            if [[ $source_mode == --gpio ]]; then
                printf 'gpiomon stopped: GPIO25 may already be owned by another process.\n' >&2
                exit 1
            fi
            break
        fi
    else
        if ! IFS= read -r -t "$window" event <&3; then
            # Bash read returns nonzero for either a timeout or EOF.
            classify "$count"
            count=0
            [[ $source_mode == --stdin ]] && break
            continue
        fi
    fi
    [[ $event == falling ]] || continue
    ((count+=1))
done
if (( count > 0 )); then
    classify "$count"
fi
