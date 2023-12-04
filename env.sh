export ONE_PROF_BASE=$(pwd)

export PATH="$PATH:$ONE_PROF_BASE/prof/fireperf"
export PATH="$PATH:$ONE_PROF_BASE/prof/fireperf/FlameGraph"

conda activate $ONE_PROF_BASE/chipyard/.conda-env
