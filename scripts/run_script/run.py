#!/usr/bin/env python3

import json
import sys
from argparse import ArgumentParser, Namespace
from dataclasses import dataclass
from enum import Enum
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
            try:
                out_default = eval(
                    f"{default['default']['type']}({default['default']['value']})")
            except NameError:
                out_default = str(default['default']['value'])

            parser.add_argument(as_argument_type,
                                default=out_default, required=False)
        else:
            parser.add_argument(as_argument_type, required=True)

    parsed = parser.parse_args(argv)

    return parsed


if __name__ == "__main__":
    current_directory = Path(__file__).parent

    forgotten_root = current_directory.parent.parent

    namespace = initialize_cli(forgotten_root, sys.argv)
