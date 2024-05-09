# Vertical profiler

## Usage

### Step 1.
- Build the FireSim driver (and bitstream) for the config that you want to run
    - We need the above step as we need the dts

### Step 2.
- Setup the workload directory
```
- test-io
        | - script-test (top level directory that the profiler will work on)
                            | - markdown (classic marshal directory structure)
                                        | - overlay/root/markdown.riscv
```

- setup the configuration json

```json
{
  "workload_base_dir" : "script-test",
  "workload" : {
    "user_bins": ["overlay/root/markdown.riscv"],
    "marshal": {
      "name": "markdown",
      "base": "br-base.json",
      "overlay": "overlay",
      "command": "cd /root && ./run.sh",
      "outputs": ["/root/trace.out"]
    }
  },
  "firesim" : {
    "config_hwdb": "alveo_u250_cospike_boom_config_70",
    "sim_dir": "/scratch/joonho.whangbo/coding/FIRESIM_RUNS_DIR/script-test",
    "cospike_threads": 16,
    "metasim": false,
    "metasim_sim": "vcs"
  },
  "profiler" : {
    "outdir":          "perf-measure",
    "spike_only_mode": false,
    "profiler_bin":    "src/builddir/profiler_main",
    "libspikedevs":    "src/spike-devices/libspikedevices.so",
    "isa":             "rv64imafdczicsr_zifencei_zihpm_zicntr",
    "reader_threads":  12
  }
}
```

### Step 3.
- Build the workload

```bash
cd scripts
./profiler workloadsetup --config ../profiler_config.json
```

- The above commands will do the following
    - Generate a marshal build script
    - Run `marshal build <config> && marshal install <config>`
    - Compile a `dtb` from the `dts` file
    - Disassemble the kernel binary so that the profiler can use it to perform out of band kernel profiling


### Step 4.
- Run FireSim

```bash
./profiler runfiresim  --config ../profiler_config.json
```

- The above commands will do the following
    - According to the `firesim` field in `profiler_config.json`, it will generate a firesim `config_runtime.yaml` and run `firesim infrasetup` && `firesim runworkload`

### Step 5.
- Run the profiler
```bash
./profiler runprofiler --config ../profiler_config.json
```

- Based on the firesim output traces, dtb, objdump etc, the profiler will run

### Step 6.
- We can auto launch and refresh the profiler visualization.
- First start the perfetto server localy (localhost:10000)
```bash
$ cd perfetto

# Install build dependencies
tools/install-build-deps --ui

# Will build into ./out/ui by default. Can be changed with --out path/
# The final bundle will be available at ./ui/out/dist/.
# The build script creates a symlink from ./ui/out to $OUT_PATH/ui/.
ui/build

# This will automatically build the UI. There is no need to manually run
# ui/build before running ui/run-dev-server.
ui/run-dev-server
```

- Now, install playwright
```bash
pip install pytest-playwright
```

- Run the below command which will generate a protobuf message from the profiler event log and feed it into the perfetto server, and refresh it periodically
```bash
./profiler display --config ../profiler_config.json
```
