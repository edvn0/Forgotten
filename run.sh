#!/bin/bash

export CMAKE_MSVC_RUNTIME_LIBRARY="MultiThreadedDebug"

[[ -v "OS_VERSION_WIN" ]]; /usr/bin/env python ./scripts/run_script/run.py "$@" && exit 0

[[ -v "OS_VERSION_MACOS" ]]; /usr/bin/env python3 ./scripts/run_script/run.py "$@" && exit 0

