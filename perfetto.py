#!/usr/bin/env python3

import os
import subprocess
import sys

dir = os.path.dirname(os.path.realpath(__file__))
config_file = os.path.join(dir, "perfetto_config.txt")
output_file = os.path.join(dir, "trace.pftrace")

if not os.path.isfile(config_file):
    print("Config file does not exist: " + config_file)
    sys.exit(1)
config = open(config_file, 'r')

subprocess.run(["adb", "shell", "rm", "-f", "/data/misc/perfetto-traces/trace"], check=False)
subprocess.run(["adb", "shell", "perfetto", "-c", "-", "--txt", "-o",
"/data/misc/perfetto-traces/trace"], stdin=config, check=True)
if os.path.isfile(output_file):
    os.remove(output_file)
subprocess.run(["adb", "pull", "/data/misc/perfetto-traces/trace", output_file], check=True)
