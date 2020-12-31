# Helper script, does nothing.
set -mex
cd $(dirname $0)

function cleanup () {
    if [[ -n "$WEBORF_PID" ]]; then
        kill -9 "$WEBORF_PID"
    fi
}
trap cleanup EXIT


function run_weborf () {
    ../weborf $@ &
    WEBORF_PID=$(jobs -p)

    # Wait for it to be ready
    if [[ $(ls /proc/$WEBORF_PID/fd/ | wc -l) -lt 4 ]]; then
        sleep 2
    fi
}
