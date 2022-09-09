#!/usr/bin/env python3

from datetime import datetime
import os
from enum import Enum
from subprocess import check_call, CalledProcessError
import sys
from argparse import ArgumentParser, Namespace
from pathlib import Path
from typing import Callable, List

import yaml


class Color(Enum):
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKCYAN = '\033[96m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'


def __message_to_coloured(message: str, color: Color, with_time=True):
    datetime_string = datetime.now().strftime("%H:%M:%S")
    if with_time:
        return f"{color}[{datetime_string}] - {message}{Color.ENDC}"
    else:
        return f"{color}{message}{Color.ENDC}"


def log_success(message: str, with_time=True):
    print(__message_to_coloured(message, Color.OKGREEN, with_time), file=sys.stdout)


def log_failure(message: str, with_time=True):
    print(__message_to_coloured(message, Color.FAIL, with_time), file=sys.stderr)


def log_info(message: str, with_time=True):
    print(__message_to_coloured(message, Color.WARNING, with_time), file=sys.stdout)


def in_directory_call_process(directory: str, process_func: Callable[[], bool]):
    wd = os.getcwd()
    os.chdir(directory)
    try:
        process_func()
    except Exception as e:
        log_failure(str(e))
        return False
    os.chdir(wd)

    return True


def initialize_cli(project_root: Path, argv: List[str] = None) -> Namespace:
    if argv is None:
        argv = []

    program_defaults: List = []

    try:
        with open(f"{project_root}/cli_defaults.yml", 'r') as f:
            program_defaults = yaml.safe_load(f)
    except IOError as e:
        log_failure(f"IOError: {e.strerror}")
        exit(1)

    parser = ArgumentParser("ForgottenEngine")

    for program_option in program_defaults:
        as_argument_type = program_option['name'] if program_option[
                                                         'type'] == 'Optional' else f"--{program_option['name']}"

        if 'default' in program_option:

            default_value = program_option['default']

            if default_value['type'] == "bool":
                parser.add_argument(as_argument_type, action='store_true')
            else:
                try:
                    out_default = eval(
                        f"{default_value['type']}({default_value['value']})")
                except NameError:
                    out_default = str(default_value['value'])
                except SyntaxError:
                    out_default = eval(
                        f"{default_value['type']}(\"{default_value['value']}\")")

                parser.add_argument(as_argument_type,
                                    default=out_default, required=False)
        else:
            parser.add_argument(as_argument_type, required=True)

    parsed = parser.parse_args(argv)

    return parsed


def build_project(build_folder: str):
    try:
        build_call = f"cmake --build . --parallel 6"
        if in_directory_call_process(
                build_folder, lambda: check_call(build_call.split(" "))):
            log_success("Built Forgotten.")
        else:
            log_failure("Could not build Forgotten.")
            exit(1)
    except CalledProcessError as e:
        log_failure(f"Could not build Forgotten, reason: \n\t\t{str(e)}")
        exit(e.returncode)


def try_install(cli_results: Namespace, build_dir: str):
    if bool(cli_results.install):
        try:
            mkdir_call = f"mkdir -p"
            mkdir_call = mkdir_call.split(" ")
            mkdir_call.append(f"{cli_results.install_path}")
            check_call(mkdir_call)
        except CalledProcessError as e:
            log_failure(
                f"Could not create install directory, reason: \n\t\t{str(e)}")

        log_success(
            f"Created install directory at {cli_results.install_path}.")

        try:
            install_call = f"cp -r {build_dir}/ForgottenApp/resources"
            install_call = install_call.split(" ")
            install_call.append(f"{cli_results.install_path}")
            check_call(install_call)
        except CalledProcessError as e:
            log_failure(
                f"Could not copy installation resources, reason: \n\t\t{str(e)}")

        log_success(f"Copied resources to {cli_results.install_path}.")

        try:
            install_call = f"cp {build_dir}/ForgottenApp/ForgottenApp"
            install_call = install_call.split(" ")
            install_call.append(f"{cli_results.install_path}")
            check_call(install_call)
        except CalledProcessError as e:
            log_failure(
                f"Could not copy Forgotten to install directory, reason: \n\t\t{str(e)}")

        log_success(f"Installed Forgotten at {cli_results.install_path}.")


def main():
    current_directory = Path(__file__).parent

    forgotten_root = current_directory.parent.parent

    try:
        copy_defaults_call = ["cp", f"{forgotten_root}/cli_defaults.yml",
                              f"{forgotten_root}/ForgottenApp/resources/cli_defaults.yml"]
        check_call(copy_defaults_call)
    except CalledProcessError as e:
        log_failure(
            f"Could not copy cli_defaults.yml to App resources, reason: \n\t\t{str(e)}")
        exit(e.returncode)

    args = sys.argv[1:]

    cli_results = initialize_cli(forgotten_root, args)

    should_clean = bool(cli_results.clean)

    build_folder = f"build-{cli_results.generator}".replace(" ", "")

    if should_clean:
        try:
            cmake_call = f"rm -rf {forgotten_root}/{build_folder}/{cli_results.build_type}"
            check_call(cmake_call.split(" "))
        except CalledProcessError as e:
            log_failure(f"Could not clean folder, reason: \n\t\t{str(e)}")
            exit(e.returncode)

    build_dir_exists = os.path.isdir(
        f"{forgotten_root}/{build_folder}/{cli_results.build_type}")

    if should_clean or not build_dir_exists or cli_results.force_regenerate:
        try:
            cmake_call = \
                f"cmake -S. -DASSIMP_BUILD_ASSIMP_TOOLS=OFF -DCMAKE_EXPORT_COMPILE_COMMANDS=1 " \
                f"-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDebug -DCMAK" \
                f"E_BUILD_TYPE={cli_results.build_type} -DFORGOTTEN_OS={cli_results.os} " \
                f"-DBUILD_SHARED_LIBS=OFF -DUSE_ALTERNATE_LINKER={cli_results.linker} " \
                f"-DASSIMP_BUILD_ZLIB=TRUE -DSPIRV_CROSS_STATIC=ON -DSPIRV_CROSS_CLI=OFF " \
                f"-DSPIRV_CROSS_ENABLE_TESTS=OFF -DSPIRV_CROSS_ENABLE_GLSL=ON -DSPIRV_CROSS_ENABLE_HLSL=OFF " \
                f"-DSPIRV_CROSS_ENABLE_MSL=OFF -DSPIRV_CROSS_ENABLE_CPP=OFF -DSPIRV_CROSS_ENABLE_REFLECT=OFF " \
                f"-DSPIRV_CROSS_ENABLE_C_API=OFF -DSPIRV_CROSS_ENABLE_UTIL=OFF -DSPIRV_CROSS_SKIP_INSTALL=ON " \
                f"-DSHADERC_ENABLE_WGSL_OUTPUT=OFF -DSHADERC_SKIP_INSTALL=ON -DSHADERC_SKIP_TESTS=ON " \
                f"-DSHADERC_SKIP_EXAMPLES=ON -DSHADERC_SKIP_COPYRIGHT_CHECK=ON " \
                f"-DSHADERC_ENABLE_WERROR_COMPILE=OFF -B{forgotten_root}/{build_folder}/" \
                f"{cli_results.build_type}"

            configure_call = cmake_call.split(" ")
            configure_call.append(f"-G{cli_results.generator}")
            check_call(configure_call)
        except CalledProcessError as e:
            log_failure(
                f"Could not configure Forgotten, reason: \n\t\t{str(e)}")
            exit(e.returncode)

    build_project(
        f"{forgotten_root}/{build_folder}/{cli_results.build_type}")

    try:
        symlink_call = f"cmake -E create_symlink {forgotten_root}/{build_folder}/{cli_results.build_type}/compile_commands.json {forgotten_root}/compile_commands.json"
        check_call(symlink_call.split(" "))
    except CalledProcessError as e:
        log_info(f"Could not symlink compile_commmands from build to root.")

    try_install(
        cli_results, f"{forgotten_root}/{build_folder}/{cli_results.build_type}")

    try:
        run_call = [
            "./ForgottenApp",
            f"--width={cli_results.width}",
            f"--name={cli_results.name}",
            f"--height={cli_results.height}",
        ] if cli_results.os != "Windows" else [
            "./ForgottenApp",
            f"--width={cli_results.width}",
            f"--name={cli_results.name}",
            f"--height={cli_results.height}",
        ]
        run_folder = f"{forgotten_root}/{build_folder}/{cli_results.build_type}/ForgottenApp"

        if cli_results.generator != "Ninja":
            run_folder += "/Debug"

        if in_directory_call_process(run_folder, lambda: check_call(run_call)):
            log_success("Running Forgotten...")
        else:
            log_failure("Could not run Forgotten")
            exit(1)
    except CalledProcessError as e:
        log_failure(f"Could not run Forgotten, reason: \n\t\t{str(e)}")
        exit(e.returncode)

    log_success("Closed Forgotten.")


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        log_success("Exiting.")
