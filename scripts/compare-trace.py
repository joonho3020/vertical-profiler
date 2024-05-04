#!/usr/bin/env python3


import subprocess
from typing import List, Tuple
from pathlib import Path
import argparse
from tqdm import trange, tqdm

parser = argparse.ArgumentParser(description='Compare the CoSpike traces to check if there are any divergence')
parser.add_argument('--ref',  type=str, required=True, help='default_simulation_dir for the reference trace')
parser.add_argument('--mine', type=str, required=True, help='default_simulation_dir for the trace that needs to be validated')
args = parser.parse_args()


class Trace:
  time: int
  iaddr: int
  valid: int
  exception: int
  interrupt: int
  has_w: int
  cause: int
  wdata: int

  def __init__(self, time: int, iaddr: int, valid: int, exception: int,
               interrupt: int, has_w: int, cause: int, wdata: int):
    self.time = time
    self.iaddr = iaddr
    self.valid = valid
    self.exception = exception
    self.interrupt = interrupt
    self.has_w = has_w
    self.cause = cause
    self.wdata = wdata

  def __str__(self):
    return f'{self.time} {self.iaddr} {self.valid} {self.exception} {self.interrupt} {self.has_w} {self.cause} {self.wdata}'

def compare_trace(me: Trace, other: Trace) -> bool:
  ret: bool = (me.time != other.time or
     me.iaddr != other.iaddr or
     me.valid != other.valid or
     me.exception != other.exception or
     me.interrupt != other.interrupt or
     me.has_w != other.has_w or
     me.cause != other.cause or
     me.wdata != other.wdata)
  return ret

def uncompress(compressed: Path, outfilename: str) -> Path:
  uncompressed = compressed.parent.joinpath(outfilename)
  subprocess.run(['gzip', '-kdf', str(compressed), '>', str(uncompressed)])
  return uncompressed

def get_lines(uncompressed: Path) -> List[str]:
  print(uncompressed)
  with open(uncompressed, 'r') as f:
    return f.readlines()

def generate_traces(uncompressed: Path) -> List[Trace]:
  lines: List[str] = get_lines(uncompressed)
  traces: List[Trace] = list()
  print(f'generating traces for {uncompressed}')
  for line in tqdm(lines):
    words = line.split()
    trace = Trace(
        int(words[1]),
        int(words[2], 16),
        int(words[3]),
        int(words[4]),
        int(words[5]),
        int(words[6]),
        int(words[7]),
        int(words[8], 16))
    traces.append(trace)
  return traces

def compare_traces(my_trace: List[Trace], ref_trace: List[Trace], ref_start: int) -> Tuple[bool, int]:
  cnt = len(my_trace)
  mismatch = False
  for i in range(ref_start, cnt):
    mine = my_trace[i]
    ref  = ref_trace[i]
    if compare_trace(mine, ref):
      mismatch = True
      print(f'mine {mine} != ref {ref}')
  return (mismatch, cnt)

def run():
# ref_comp = Path(args.ref).joinpath('sim_slot_0', 'COSPIKE-TRACE-0.gz')
# ref_uncomp = uncompress(ref_comp, 'COSPIKE-TRACE-0')
  ref_uncomp = Path(args.ref).joinpath('sim_slot_0', 'COSPIKE-TRACE-0')
  ref_traces = generate_traces(ref_uncomp)

  ref_start = 0
  mine_sim_slot = Path(args.mine).joinpath('sim_slot_0')
  mine_uncomp_cnt = len(list(mine_sim_slot.glob('COSPIKE-TRACE-0-*')))
  print(f'mine_uncomp_cnt {mine_uncomp_cnt}')

  print('Comparing traces')
  for i in trange(1, mine_uncomp_cnt):
    mine_comp = mine_sim_slot.joinpath(f'COSPIKE-TRACE-0-{i}.gz')
    mine_uncomp = uncompress(mine_comp, f'COSPIKE-TRACE-0-{i}')
    my_traces = generate_traces(mine_uncomp)

    (mismatch, cnt) = compare_traces(my_traces, ref_traces, ref_start)
    ref_start += cnt
    if mismatch:
      exit(1)
  print("Match!")


def main():
  run()

if __name__=="__main__":
  main()
