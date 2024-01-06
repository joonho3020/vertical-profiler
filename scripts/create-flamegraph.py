#!/usr/bin/env python3


import argparse
import os
import sys

def bash(cmd):
  fail = os.system(cmd)
  if fail:
    print(f'[*] failed to execute {cmd}')
    sys.exit(1)
  else:
    print(cmd)

parser = argparse.ArgumentParser(description='Generate a flamegraph')
parser.add_argument('--callstack', '-c', type=str, default='SPIKE-CALLSTACK', help='callstack output file from the profiler')
args = parser.parse_args()


def main():
  print("Generating flame graph")
  bash(f"stackcollapse-tracerv.py {args.callstack} | \
       tee {args.callstack}.fold | \
       flamegraph.pl --title=\"Flame Graph\" --fontsize 16 --height 20 --bgcolors \"#ffffff\" --countname cycles > {args.callstack}.flamegraph.svg")

  print("Generating icicle graph")
  bash(f"cat {args.callstack}.fold | \
       flamegraph.pl --title=\"Flame Graph\" --inverted --colors blue --fontsize 16 --height 20 --bgcolors \"#ffffff\" --countname cycles > {args.callstack}.icicle.svg")

if __name__ == '__main__':
  main()
