#!/usr/bin/env python3

import yaml
import json
import subprocess
from pathlib import Path
from typing import Dict, List
import os

PROFILER_BASEDIR = Path(os.environ['PROFILER_BASEDIR'])

class AbstractConfig:
  base_dir: Path

  def __init__(self, base_dir: Path):
    self.base_dir = base_dir

# base_dir
#    - name
#       - overlay
#    - name.yaml
class WorkloadConfig(AbstractConfig):
  base_dir: Path
  name: str
  user_bins: List[Path]
  marshal: Dict

  def __init__(self, base_dir: Path, workload_cfg: Dict):
    super().__init__(base_dir)
    self.marshal = workload_cfg['marshal']
    self.name = self.marshal['name']
    self.user_bins = [
        base_dir.joinpath(self.name, x)
        for x in workload_cfg['user_bins']]

  def firechip_dir(self) -> Path:
    return self.base_dir.joinpath(f'{self.name}-firechip')

  def generate_json(self) -> Path:
    self.base_dir.mkdir(parents=True, exist_ok=True)
    json_path = self.base_dir.joinpath(f'{self.name}.json')
    with open(json_path, 'w') as f:
      json.dump(self.marshal, f, indent=4)
    return json_path

  def check_marshal_cmd(self) -> int:
    ret = subprocess.run(['which', 'marshal'])
    return ret.returncode

  def call_marshal(self, cmd: str, json_path: Path) -> None:
    if self.check_marshal_cmd() != 0:
      print('marshal command not found')
      exit(1)

    ret = subprocess.run(['marshal', cmd, str(json_path)])
    if ret.returncode != 0:
      print(f'marshal {cmd} failed')
      exit(1)

  def build_and_install_marshal(self) -> None:
    marshal_json: Path = self.generate_json()
    self.call_marshal('build',   marshal_json)
    self.call_marshal('install', marshal_json)

  def link_marshal_builds(self) -> None:
    chipyard_dir = PROFILER_BASEDIR.joinpath('chipyard')
    firechip_image_dir = chipyard_dir.joinpath('software/firemarshal/images/firechip')
    built_workload_dir = firechip_image_dir.joinpath(f'{self.name}')

    ret = subprocess.run(['ln', '-sf', str(built_workload_dir), str(self.firechip_dir())])
    if ret.returncode != 0:
      print(f'linking marshal builds failed')
      exit(1)

  def firechip_bin(self) -> Path:
    return self.firechip_dir().joinpath(f'{self.name}-bin')

  def firechip_bin_dwarf(self) -> Path:
    return self.firechip_dir().joinpath(f'{self.name}-bin-dwarf')

  def firechip_bin_dwarf_dump(self) -> Path:
    return self.firechip_dir().joinpath('KERNEL.riscv.dump')

  def firechip_image(self) -> Path:
    return self.firechip_dir().joinpath(f'{self.name}.img')

  def disasm_kernel(self):
    with open(self.firechip_bin_dwarf_dump(), 'w') as f:
      ret = subprocess.run(['riscv64-unknown-linux-gnu-objdump',
                            '-D',
                            str(self.firechip_bin_dwarf())],
                           stdout=f)
    if ret.returncode != 0:
      print(f'Disassembling the kernel failed')
      exit(1)

class FireSimTargetConfig(WorkloadConfig):
  config_hwdb: str
  platform: str
  platform_config: str
  target_config: str

  def __init__(self, base_dir: Path, cfg: Dict):
    super().__init__(base_dir, cfg['workload'])
    self.config_hwdb = cfg['firesim']['config_hwdb']

    with open(self.config_build_recipes(), 'r') as f:
      build_recipes = yaml.safe_load(f)

    self.platform = build_recipes[self.config_hwdb]['PLATFORM']
    self.platform_config = build_recipes[self.config_hwdb]['PLATFORM_CONFIG']
    self.target_config = build_recipes[self.config_hwdb]['TARGET_CONFIG']

  def cfg_name(self) -> str:
    return f'{self.platform}-firesim-FireSim-{self.target_config}-{self.platform_config}'

  def firesim_dir(self) -> Path:
    return PROFILER_BASEDIR.joinpath('chipyard', 'sims', 'firesim')

  def generated_src_dir(self) -> Path:
    return self.firesim_dir().joinpath('sim', 'generated-src', self.platform, self.cfg_name())

  def config_build_recipes(self) -> Path:
    return self.firesim_dir().joinpath('deploy', 'config_build_recipes.yaml')

  def dts(self) -> Path:
    dts_filename = f'firesim.firesim.FireSim.{self.target_config}.dts'
    return self.generated_src_dir().joinpath(dts_filename)

  def dtb(self) -> Path:
    return self.base_dir.joinpath('target.dtb')

  def build_dtb(self):
    ret = subprocess.run(['dtc', '-O', 'dtb', '-o', str(self.dtb()), str(self.dts())])
    if ret.returncode != 0:
      print('build dtb failed')
      exit(1)

class FireSimRuntimeConfig(FireSimTargetConfig):
  default_sim_dir: str

  metasimulation_enabled: bool
  metasimulation_host_simulator: str

  default_hw_config: str
  plusargs_passthrough: str

  workload: str

  base_config_runtime: Path

  def __init__(self, base_dir: Path, cfg: Dict):
    super().__init__(base_dir, cfg)
    self.base_config_runtime = self.firesim_dir().joinpath('deploy', 'config_runtime.yaml')

    cfg_firesim = cfg['firesim']
    self.default_sim_dir = cfg_firesim['sim_dir']
    self.metasimulation_enabled = cfg_firesim['metasim']
    self.metasimulation_host_simulator = cfg_firesim['metasim_sim']
    self.default_hw_config = cfg_firesim['config_hwdb']

    cospike_threads = cfg_firesim['cospike_threads']
    self.plusargs_passthrough = f'+cospike-trace={cospike_threads}'
    self.workload = self.name + '.json'

  def dump_config_runtime(self) -> Path:
    out_file = self.base_dir.joinpath('config_runtime.yaml')

    with open(self.base_config_runtime, 'r') as f:
      base_config = yaml.safe_load(f)
    new_config = base_config

    if self.metasimulation_enabled:
      new_config['run_farm']['recipe_arg_overrides']['run_farm_hosts_to_use'] = [{
            "localhost": "four_metasims_spec"
          }]
    else:
      new_config['run_farm']['recipe_arg_overrides']['run_farm_hosts_to_use'] = [{
            "localhost": "one_fpgas_spec"
          }]

    new_config['run_farm']['recipe_arg_overrides']['default_simulation_dir'] = self.default_sim_dir

    new_config['metasimulation']['metasimulation_enabled'] = self.metasimulation_enabled
    new_config['metasimulation']['metasimulation_host_simulator'] = self.metasimulation_host_simulator

    new_config['target_config']['default_hw_config'] = self.default_hw_config
    new_config['target_config']['plusarg_passthrough'] = self.plusargs_passthrough
    new_config['workload']['workload_name'] = self.workload

    new_config['host_debug']['zero_out_dram'] = False if self.metasimulation_enabled else True

    with open(out_file, 'w') as f:
      yaml.dump(new_config, f)

    return out_file

  def run_firesim_command(self, command: str, config_runtime: Path):
    ret = subprocess.run(['firesim', command, '-c', str(config_runtime)])
    if ret.returncode != 0:
      print(f'failed to execute firesim {command} -c {str(config_runtime)}')
      exit(1)

  def run_firesim(self):
    config_runtime = self.dump_config_runtime()
    self.run_firesim_command('infrasetup',  config_runtime)
    self.run_firesim_command('runworkload', config_runtime)

class ProfilerConfig(FireSimRuntimeConfig):
  spike_only_mode: bool
  prof_bin: Path
  prof_out: Path
  spike_logs: Path
  spike_device_libs: Path
  isa: str
  reader_threads: int

  def __init__(self, base_dir: Path, config: Dict):
    super().__init__(base_dir, config)

    prof_config = config['profiler']

    self.prof_out = self.base_dir.joinpath(prof_config['outdir'])
    self.prof_bin = PROFILER_BASEDIR.joinpath(prof_config['profiler_bin'])

    self.spike_only_mode = prof_config['spike_only_mode']
    self.spike_logs = self.base_dir.joinpath('SPIKE-LOGS')
    self.spike_device_libs = PROFILER_BASEDIR.joinpath(prof_config['libspikedevs'])

    self.isa = prof_config['isa']
    self.reader_threads = prof_config['reader_threads']

  def profiler_run_command(self) -> List[str]:
    base_cmd_list = [
        str(self.prof_bin),
        f'--log={self.spike_logs}',
        f'--prof-out={self.prof_out}',
        f'--extlib={self.spike_device_libs}',
        f'--device=iceblk,img={self.firechip_image()}',
        f'--kernel-info={self.firechip_bin_dwarf_dump()},{self.firechip_bin_dwarf()}'
    ]
    user_bin_info = [f'--user-info={x},{x}' for x in self.user_bins]

    profiler_logs = self.prof_out.joinpath('PROFILER-LOGS')
    workload = [f'{self.firechip_bin()}']

    if self.spike_only_mode:
      cmdlist = base_cmd_list + user_bin_info + workload
    else:
      sim_slot_dir = Path(self.default_sim_dir).joinpath('sim_slot_0')
      trace_dir = sim_slot_dir.joinpath('COSPIKE-TRACES')

      # HACK:
      cospike_config = sim_slot_dir.joinpath('COSPIKE-CONFIG')
      uncompressed_buffer_bytes = 0
      with open(cospike_config, 'r') as f:
        lines = f.readlines()
        for line in lines:
          words = line.split()
          uncompressed_buffer_bytes = int(words[3])
          break
      insn_per_file = uncompressed_buffer_bytes / 50
      trace_cmds = [
          f'--device=sifive_uart',
          f'--dtb={self.dtb()}',
          f'--isa={self.isa}',
          f'--rtl-cfg=\"{trace_dir}:{self.reader_threads}:{insn_per_file}:{uncompressed_buffer_bytes}\"'
      ]
      cmdlist = base_cmd_list + user_bin_info + trace_cmds + workload
    return cmdlist

  def run_profiler(self):
    cmd: List[str] = self.profiler_run_command()

    run_script = self.base_dir.joinpath('run.sh')
    with open(run_script, 'w') as f:
      f.write(' \\\n'.join(cmd))
    run_script.chmod(0o777)

    self.prof_out.mkdir(parents=True, exist_ok=True)
    os.chdir(self.base_dir)
    subprocess.run('./run.sh', shell=True)

  def display(self):
    event_logs = self.prof_out.joinpath('PROF-EVENT-LOGS')
    perfetto_dir = PROFILER_BASEDIR.joinpath('src', 'perfetto')
    os.chdir(perfetto_dir)
    cwd = os.getcwd()
    print(cwd)

    event_logs_rel = Path('../../').joinpath(event_logs.relative_to(PROFILER_BASEDIR))
    print(event_logs_rel)


    event_logs_proto = self.prof_out.joinpath('PROF-EVENT-LOGS.proto')
    with open(event_logs_rel, 'r') as stdin:
      with open(event_logs_proto, 'w') as stdout:
        subprocess.run(['protoc',
                        '--encode=perfetto.protos.Trace',
                        'protos/perfetto/trace/trace.proto'],
                        stdin=stdin, stdout=stdout)
