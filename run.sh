#! /usr/bin/env bash

pushd () {
    command pushd "$@" > /dev/null
}

popd () {
    command popd > /dev/null
}

copy_resources() {
    mkdir -p ./ForgottenEngine/resources
    cp -r ../../ForgottenEngine/resources ./ForgottenEngine/
    cp -r ../../ForgottenApp/resources ./ForgottenApp/
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
        local root_folder="../.."
        cmake "$root_folder" -G Ninja -DCMAKE_BUILD_TYPE="$build_type"
    fi;

    copy_resources

    ninja "$target" -j 4
    popd
}

start_target() {
    pushd "$1"

    local name="$1"

    if ! [ "$TARGET" == "ForgottenEngine" ]
    then
        shift
        exec "../$build_folder/$name/$name" "$@"
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
if [ "$TARGET" == "ForgottenApp" ];
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

