#!/usr/bin/env python3

import sys
import subprocess

args = []

for arg in sys.argv[1:]:
    if arg.startswith('--prefix='):
        subst = arg[9:].replace('-', '_')
        arg = arg[:9] + subst
    args.append(arg)

subprocess.call(args)
