# /usr/bin/env bash

pushd () {
    command pushd "$@" > /dev/null
}

popd () {
    command popd > /dev/null
}

configure_visual_studio() {
    mkdir -p VisualStudio
    pushd VisualStudio
    cmake .. -G "Visual Studio 17 2022" 
    popd
}

configure_visual_studio
