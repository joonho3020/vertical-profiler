#!/usr/bin/env python3

import argparse
import os
import utils

parser = argparse.ArgumentParser(description='Generate a protobuf trace for perfetto')
parser.add_argument('--perfetto-trace', '-p', type=str, default='PROF-LOGS-THREADPOOL', help='raw trace from the profiler')
parser.add_argument('--out-file',       '-o', type=str, default='profiler.perfetto',    help='output protobuf file name')
args = parser.parse_args()

def main():
  trace_path = os.path.join(os.getcwd(), args.perfetto_trace)
  os.chdir("../src/perfetto")
  utils.bash(f"protoc \
      --encode=perfetto.protos.Trace \
      protos/perfetto/trace/trace.proto \
      < {trace_path} > {args.out_file}")
  utils.bash(f"mv {args.out_file} ../builddir/out")

if __name__ == "__main__":
  main()
