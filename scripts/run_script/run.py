#!/usr/bin/env python3

import os
import json
import sys
from argparse import ArgumentParser, Namespace
from pathlib import Path
from typing import Callable, List


def in_directory_call_process(directory: str, process_func: Callable[[None], bool]):
    wd = os.getcwd()
    os.chdir(directory)
    try:
        process_func()
    except Exception as e:
        return False
    os.chdir(wd)

    return True


def initialize_cli(project_root: Path, argv: List[str] = None) -> Namespace:
    if argv is None:
        argv = []

    program_defaults: List = []

    try:
        with open(f"{project_root}/cli_defaults.json", 'r') as f:
            program_defaults = json.load(f)
    except IOError as e:
        print(f"IOError: {e.strerror}", file=sys.stderr)
        exit(0)

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


if __name__ == "__main__":
    current_directory = Path(__file__).parent

    forgotten_root = current_directory.parent.parent

    args = sys.argv[1:]

    cli_results = initialize_cli(forgotten_root, args)

    from subprocess import check_call, CalledProcessError

    did_clean = bool(cli_results.clean)

    if did_clean:
        try:
            cmake_call = f"rm -rf {forgotten_root}/build/{cli_results.build_type}"
            check_call(cmake_call.split(" "))
        except CalledProcessError as e:
            print(
                f"Could not clean folder, reason: \n\t\t{str(e)}", file=sys.stderr)
            exit(e.returncode)

    if did_clean:
        try:
            cmake_call = f"cmake {forgotten_root} -GNinja -DCMAKE_BUILD_TYPE={cli_results.build_type} -B{forgotten_root}/build/{cli_results.build_type}"
            check_call(cmake_call.split(" "))
        except CalledProcessError as e:
            print(
                f"Could not configure Forgotten, reason: \n\t\t{str(e)}", file=sys.stderr)
            exit(e.returncode)

    try:
        build_call = "ninja -j8"
        build_folder = f"{forgotten_root}/build/{cli_results.build_type}"
        if in_directory_call_process(
                build_folder, lambda: check_call(build_call.split(" "))):
            print("Built Forgotten.")

    except CalledProcessError as e:
        print(
            f"Could not build Forgotten, reason: \n\t\t{str(e)}", file=sys.stderr)
        exit(e.returncode)

    try:
        run_call = [
            "exec"
            "./ForgottenApp",
            "--width",
            f"{cli_results.width}",
            "--name",
            f"{cli_results.name}",
            "--height",
            f"{cli_results.height}",
        ]
        run_folder = f"{forgotten_root}/build/{cli_results.build_type}/ForgottenApp"

        if in_directory_call_process(run_folder, lambda: check_call(run_call)):
            print("Running Forgotten...")
    except CalledProcessError as e:
        print(
            f"Could not run Forgotten, reason: \n\t\t{str(e)}", file=sys.stderr)
        exit(e.returncode)
