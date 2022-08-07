#!/usr/bin/env python3

import json
import sys
from argparse import ArgumentParser, Namespace
from pathlib import Path
from typing import List


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
    if cli_results.clean:
        try:
            cmake_call = f"rm -rf {forgotten_root}/build/{cli_results.build_type}"
            check_call(cmake_call.split(" "))
        except CalledProcessError as e:
            print(
                f"Could not clean folder, reason: \n\t\t{str(e)}", file=sys.stderr)
            exit(e.returncode)

    try:
        cmake_call = f"cmake {forgotten_root} -GNinja -B{forgotten_root}/build/{cli_results.build_type} -DCMAKE_BUILD_TYPE={cli_results.build_type}"
        check_call(cmake_call.split(" "))
    except CalledProcessError as e:
        print(
            f"Could not continue building Forgotten, reason: \n\t\t{str(e)}", file=sys.stderr)
        exit(e.returncode)

    try:
        run_call = [
            "cd",
            f"{forgotten_root}/build/{cli_results.build_type}/ForgottenApp",
            "&&",
            "./ForgottenApp"
            "--width",
            f"{cli_results.width}",
            "--name",
            f"{cli_results.name}",
            "--height",
            f"{cli_results.height}",
        ]

        check_call(run_call)
    except CalledProcessError as e:
        print(
            f"Could not run Forgotten, reason: \n\t\t{str(e)}", file=sys.stderr)
        exit(e.returncode)
