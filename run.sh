#! /usr/bin/env bash

pushd () {
    command pushd "$@" > /dev/null
}

popd () {
    command popd > /dev/null
}

set -e

build_target() {
    local build_type="$2"
    local target="$1"

    build_folder="build/$build_type"
    if ! [ -d "$build_folder" ];
    then
        mkdir -p "$build_folder"
    fi;

    pushd "$build_folder"
    if ! [ -a "build.ninja" ];
    then
        cmake ../.. -G Ninja -DCMAKE_BUILD_TYPE="$build_type"
    fi;

    ninja "$target" -j 10
    popd
}

start_target() {
    pushd "$1"

    if ! [ "$TARGET" == "Forgotten" ]
    then
        exec "../$build_folder/$1/$1" "$@"
    fi;
 
    popd
}

TARGET="${1:-ForgottenEngine}"
BUILD_TYPE="${2:-Debug}"
CLEAN_BUILD="${3:-NoClean}"

if [ "$CLEAN_BUILD" == "Clean" ];
then
  echo "Cleaning build folder... 'rm -rf $(pwd)/build'"
  rm -rf "$(pwd)/build"
  echo "Done."
fi;

found_build_type=0
for t in {"Debug","Default","MinSizeRel","Release","RelWithDebInfo"};
do
    if [ "$t" == "$BUILD_TYPE" ];
    then
        found_build_type=1
        break
    fi;
done

found_target=0
if [ "$TARGET" == "ForgottenEngine" ];
then
    found_target=1
fi;


if [ "$found_build_type" -eq 1 ] && [ "$found_target" -eq 1 ];
then
    tgt="$TARGET"
    build_target "$tgt" "$BUILD_TYPE"
    shift 2
    start_target "$tgt" "$@"
else
    if ! [ "$found_build_type" -eq 1 ];
    then
        echo "Build mode \"$BUILD_TYPE\" not applicable."
        echo "Accepted are: 'Debug', 'Default', 'MinSizeRel', 'Release' and 'RelWithDebInfo'"
        exit 1
    fi;

    if ! [ "$found_target" -eq 1 ];
    then
        echo "Target \"$TARGET\" not applicable."
        echo "Accepted are: 'ForgottenEngine'"
        exit 1
    fi;
fi;

