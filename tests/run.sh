#!/bin/sh
# Test entry point.
#   tests/run.sh                 host C unit tests + Python tool tests
#   tests/run.sh device <port> [subsystem ...]
#                                flash the board and check its boot markers
#                                (needs the ESP-IDF environment, see README)
set -e
cd "$(dirname "$0")"

if [ "$1" = device ]; then
    [ -n "$2" ] || { echo "usage: $0 device <port> [subsystem ...]" >&2; exit 2; }
    [ -n "$IDF_PYTHON_ENV_PATH" ] || { echo "ESP-IDF environment not loaded (export.sh)" >&2; exit 2; }
    shift
    exec "$IDF_PYTHON_ENV_PATH/bin/python" device/check_boot.py "$@"
fi

cmake -S host -B host/build -G Ninja >/dev/null
cmake --build host/build
ctest --test-dir host/build --output-on-failure
python3 -m unittest discover -s tools -t ..
