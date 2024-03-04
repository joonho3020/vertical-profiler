#!/usr/bin/env python3

import argparse
import os
import utils
import json
from typing import Dict

parser = argparse.ArgumentParser(description="Helper script to run the profiler code")
parser.add_argument('--config',    '-c', type=str,  required=True,  help='json file containing the run config')
parser.add_argument('--print-cmd', '-p', action='store_true',       help='Simply print the profiler run command')
args = parser.parse_args()

def open_json(file_path: str) -> Dict:
  with open(file_path, 'r') as f:
    data = json.load(f)
  return data

def profiler_run_cmd(config: Dict) -> str:
  outdir      = config['outdir']
  spike_only_mode = config['spike_only_mode']
  prof_bin    = config['profiler_bin']
  spike_logs  = f"--log={os.path.join(outdir, config['spike_log'])}"
  prof_out    = f"--prof-out={config['outdir']}"
  kernel_info = f"--kernel-info=\"{config['kernel_dump']},{config['kernel_dwarf']}\""
  spikelib    = f"--extlib={config['libspikedevs']}"
  iceblk      = f"--device=iceblk,img={config['iceblk_img']}"
  workload    = f"{config['bin']} | tee {os.path.join(outdir, 'PROFILER-LOGS')}"

  base_cmd_list = [prof_bin, spike_logs, prof_out, kernel_info, spikelib, iceblk]

  if spike_only_mode:
    cmdlist = base_cmd_list + [workload]
  else:
    sifive_uart = f"--device=sifive_uart"
    dtb         = f"--dtb={config['dtb']}"
    isa         = f"--isa={config['isa']}"
    rtl_trace   = f"--rtl-trace={config['rtl_trace']}"
    rtl_cmd_list = [sifive_uart, dtb, isa, rtl_trace]
    cmdlist = base_cmd_list + rtl_cmd_list + [workload]
  cmd = ' '.join(cmdlist)
  return cmd

def perfetto_proto_generation_cmd(outdir):
  cmd = f"""./scripts/create-perfetto-proto.py \
    --out-dir={outdir}"""
  return cmd

def main():
  config = open_json(args.config)

  cmd = profiler_run_cmd(config)
  if args.print_cmd:
    print(cmd)
    return
  else:
    outdir = config['outdir']
    if not os.path.exists(outdir):
      os.mkdir(outdir)
      os.mkdir(os.path.join(outdir, 'traces'))

    utils.bash(cmd)

    protogen_cmd = perfetto_proto_generation_cmd(outdir)
    utils.bash(protogen_cmd)


if __name__=="__main__":
  main()
