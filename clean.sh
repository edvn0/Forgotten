#!/bin/bash

clean() {
  local currentDir=$(pwd)
  rm -rf $(find "$currentDir" -name "*build*" -type d -depth 1)
  rm -rf "$currentDir/Xcode"
}

clean
