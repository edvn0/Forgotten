#!/usr/bin/env bash

export CMAKE_MSVC_RUNTIME_LIBRARY="MultiThreadedDebug"

if ! [[ -z ${FORGOTTEN_OS+x} ]];
then
  export FORGOTTEN_OS="MacOS"
fi

if [[ "$FORGOTTEN_OS" == "MacOS" ]];
then
  /usr/bin/env python3 ./scripts/run_script/run.py "$@"
  exit 0
elif [[ "$FORGOTTEN_OS" == "Windows" ]];
then
  /usr/bin/env python3 ./scripts/run_script/run.py "$@"
  exit 0
fi

