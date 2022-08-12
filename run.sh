#!/usr/bin/env bash

if ! [[ -z ${FORGOTTEN_OS+x} ]];
then
  echo "Found a defined OS."
else
  export FORGOTTEN_OS="MacOS"
fi

if [[ "$FORGOTTEN_OS" == "MacOS" ]];
then
  /usr/bin/env python3 ./scripts/run_script/run.py "$@"
  exit 0
elif [[ "$FORGOTTEN_OS" == "Windows" ]];
then
  /usr/bin/env python ./scripts/run_script/run.py "$@"
  exit 0
fi

