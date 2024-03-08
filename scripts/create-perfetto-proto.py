#!/usr/bin/env python3

import argparse
import os
import utils

parser = argparse.ArgumentParser(description='Generate a protobuf trace for perfetto')
parser.add_argument('--perfetto-trace', '-p', type=str, default='PROF-LOGS-THREADPOOL', help='raw trace file from the profiler')
parser.add_argument('--out-file',       '-f', type=str, default='profiler.perfetto',    help='output protobuf file name')
parser.add_argument('--out-dir',        '-d', type=str, default='out',                  help='output directory name')
args = parser.parse_args()

def generate_proto(trace_path, out_file):
  utils.bash(f"protoc \
      --encode=perfetto.protos.Trace \
      protos/perfetto/trace/trace.proto \
      < {trace_path} > {out_file}")

def main():
  trace_path = os.path.join(os.getcwd(), args.out_dir, args.perfetto_trace)
  os.chdir("src/perfetto")
  generate_proto(trace_path, args.out_file)
  os.chdir("../../")
  utils.bash(f"mv src/perfetto/{args.out_file} {args.out_dir}/")

if __name__ == "__main__":
  main()
