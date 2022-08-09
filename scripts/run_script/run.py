#!/usr/bin/env python3

from datetime import datetime
import os
from subprocess import check_call, CalledProcessError
import sys
from argparse import ArgumentParser, Namespace
from pathlib import Path
from typing import Callable, List

import yaml


class Color:
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


def in_directory_call_process(directory: str, process_func: Callable[[None], bool]):
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

    for default in program_defaults:
        as_argument_type = default['name'] if default[
            'type'] == 'Optional' else f"--{default['name']}"

        if 'default' in default:

            default_value = default['default']

            if default_value['type'] == "bool":
                parser.add_argument(as_argument_type, action='store_true')
            else:
                try:
                    out_default = eval(
                        f"{default_value['type']}({default_value['value']})")
                except NameError:
                    out_default = str(default_value['value'])

                parser.add_argument(as_argument_type,
                                    default=out_default, required=False)
        else:
            parser.add_argument(as_argument_type, required=True)

    parsed = parser.parse_args(argv)

    return parsed


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

    did_clean = bool(cli_results.clean)

    if did_clean:
        try:
            cmake_call = f"rm -rf {forgotten_root}/build/{cli_results.build_type}"
            check_call(cmake_call.split(" "))
        except CalledProcessError as e:
            log_failure(f"Could not clean folder, reason: \n\t\t{str(e)}")
            exit(e.returncode)

    build_folder_name = f"build-{cli_results.generator}".replace(" ", "")

    build_dir_exists = os.path.isdir(
        f"{forgotten_root}/build/{cli_results.build_type}")

    if did_clean or not build_dir_exists or cli_results.force_regenerate:
        try:
            cmake_call = f"cmake -DCMAKE_BUILD_TYPE={cli_results.build_type} -DUSE_ALTERNATE_LINKER={cli_results.linker} -DSPIRV_CROSS_STATIC=ON -DSHADERC_SKIP_TESTS=ON -DSHADERC_SKIP_INSTALL=ON -DSHADERC_SKIP_EXAMPLES=ON -DSHADERC_SKIP_COPYRIGHT_CHECK=ON -B{forgotten_root}/{build_folder_name}/{cli_results.build_type}"
            configure_call = cmake_call.split(" ")
            configure_call.append(f"-G{cli_results.generator}")
            check_call(configure_call.split(" "))
        except CalledProcessError as e:
            log_failure(
                f"Could not configure Forgotten, reason: \n\t\t{str(e)}")
            exit(e.returncode)

    build_project(f"{build_folder}/{cli_results.build_type}", cli_results.generator)

    try:
        build_call = "ninja -j8"
        build_folder = f"{forgotten_root}/build/{cli_results.build_type}"
        if in_directory_call_process(
                build_folder, lambda: check_call(build_call.split(" "))):
            log_success("Built Forgotten.")
        else:
            log_failure("Could not build Forgotten.")
            exit(1)
    except CalledProcessError as e:
        log_failure(f"Could not build Forgotten, reason: \n\t\t{str(e)}")
        exit(e.returncode)

    generator = str(cli_results.generator)
    if generator.count("Visual Studio") == 0:
        print("We are not using Visual Studio")
        try:
        run_call = [
            "./ForgottenApp",
            f"--width={cli_results.width}",
            f"--name={cli_results.name}",
            f"--height={cli_results.height}",
        ]
        run_folder = f"{forgotten_root}/build/{cli_results.build_type}/ForgottenApp"

        if in_directory_call_process(run_folder, lambda: check_call(run_call)):
            log_success("Running Forgotten...")
        else:
            log_failure("Could not run Forgotten")
            exit(1)
    except CalledProcessError as e:
        log_failure(f"Could not run Forgotten, reason: \n\t\t{str(e)}")
        exit(e.returncode)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        log_success("Exiting.")
