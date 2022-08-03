# /usr/bin/env bash

pushd () {
    command pushd "$@" > /dev/null
}

popd () {
    command popd > /dev/null
}

configure_xcode() {
    mkdir -p Xcode
    pushd Xcode
    cmake .. -G Xcode

    local CYAN='\033[0;32m'
    local NC='\033[0m'

    printf "${CYAN}Ouputting current schemes${NC}\n"
    exec xcodebuild -list -project Forgotten.xcodeproj
    printf "${CYAN}Done${NC}\n"
    
    popd
}

configure_xcode
