#!/usr/bin/env python
# -*- coding: utf-8 -*-

import sys
import os
import stat
import os.path
import time
import signal
import struct
import re

def error_and_exit(condition, s):
    if condition:
        print(s)
        exit(1)

error_and_exit(len(sys.argv) != 2, "Usage: python printStatus.py $pid")

shellpath = os.path.dirname(os.path.abspath(__file__))
projectpath = os.path.dirname(shellpath)
configfile = os.path.join(projectpath, "config/server.cfg")

error_and_exit(not os.path.isfile(configfile), "Cannot find config file server.cfg")

with open(configfile) as f:
    content = f.read()

match = re.findall("run_path=(.*)\n", content)

error_and_exit(not match, "Cannot find run_path=$path in config file")

run_path = match[0]

pid = int(sys.argv[1])
pidfile = os.path.join(run_path, "csiebox_server.%s" % (pid))

error_and_exit(
    not stat.S_ISFIFO(os.stat(pidfile).st_mode),
    "Cannot find fifo file: %s" % (pidfile))

print("Listen to file %s" % (pidfile))

while True:
    os.kill(pid, signal.SIGUSR1)
    with open(pidfile) as fifo:
        data = fifo.read().encode()
    threadNum = struct.unpack(">I", data)
    print("Active Thread Number %d" % (threadNum))
    time.sleep(1)
