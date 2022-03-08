# Helper script, does nothing.
set -mex
cd $(dirname $0)

if [ -n "$AUTOPKGTEST_TMP" ]; then
    # Run system weborf in autopkgtest
    BINNAME=weborf
else
    BINNAME=../weborf
fi

function cleanup () {
    if [[ -n "$WEBORF_PID" ]]; then
        kill -9 "$WEBORF_PID"
    fi
}
trap cleanup EXIT


function run_weborf () {
    "$BINNAME" $@ &
    WEBORF_PID=$(jobs -p)

    sleep 0.2
    # Wait for it to be ready
    if [[ $(ls /proc/$WEBORF_PID/fd/ | wc -l) -lt 4 ]]; then
        sleep 2
    fi
}
