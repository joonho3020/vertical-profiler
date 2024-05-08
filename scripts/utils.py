import os
import sys
import json
from typing import Dict

def bash(cmd):
  fail = os.system(cmd)
  if fail:
    print(f'[*] failed to execute {cmd}')
    sys.exit(1)
  else:
    print(cmd)

def open_json(file_path: str) -> Dict:
  with open(file_path, 'r') as f:
    data = json.load(f)
  return data
