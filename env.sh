export ONE_PROF_BASE=$(pwd)

export PATH="$PATH:$ONE_PROF_BASE/prof/fireperf"
export PATH="$PATH:$ONE_PROF_BASE/prof/fireperf/FlameGraph"

conda activate $ONE_PROF_BASE/chipyard/.conda-env
export PROTOBUF_INSTALL_DIR=$ONE_PROF_BASE/prof/protobuf/install
export PATH="$PROTOBUF_INSTALL_DIR/bin:$PATH"

export GCC="$CONDA_PREFIX/bin/x86_64-conda-linux-gnu-gcc"
export GCC_RANLIB="$CONDA_PREFIX/bin/x86_64-conda-linux-gnu-gcc-ranlib"
export GCC_AR="$CONDA_PREFIX/bin/x86_64-conda-linux-gnu-gcc-ar"
export GCC_NM="$CONDA_PREFIX/bin/x86_64-conda-linux-gnu-gcc-nm"

export GXX="$CONDA_PREFIX/bin/x86_64-conda-linux-gnu-g++"

export CC="$CONDA_PREFIX/bin/x86_64-conda-linux-gnu-cc"
export CFLAGS="-march=nocona -mtune=haswell -ftree-vectorize -fPIC -fstack-protector-strong -fno-plt -O2 -ffunction-sections -pipe -isystem $CONDA_PREFIX/include"
export CC_FOR_BUILD="$CONDA_PREFIX/bin/x86_64-conda-linux-gnu-cc"

export CXX="$CONDA_PREFIX/bin/x86_64-conda-linux-gnu-c++"
export CXXFLAGS="-fvisibility-inlines-hidden -fmessage-length=0 -march=nocona -mtune=haswell -ftree-vectorize -fPIC -fstack-protector-strong -fno-plt -O2 -ffunction-sections -pipe -isystem $CONDA_PREFIX/include"
export CXXFILT="$CONDA_PREFIX/bin/x86_64-conda-linux-gnu-c++filt"
export CXX_FOR_BUILD="$CONDA_PREFIX/bin/x86_64-conda-linux-gnu-c++"
export DEBUG_CXXFLAGS="-fvisibility-inlines-hidden -fmessage-length=0 -march=nocona -mtune=haswell -ftree-vectorize -fPIC -fstack-protector-all -fno-plt -Og -g -Wall -Wextra -fvar-tracking-assignments -ffunction-sections -pipe -isystem $CONDA_PREFIX/include"

export CPP="$CONDA_PREFIX/bin/x86_64-conda-linux-gnu-cpp"
export CPPFLAGS="-DNDEBUG -D_FORTIFY_SOURCE=2 -O2 -isystem $CONDA_PREFIX/include"
export DEBUG_CPPFLAGS="-D_DEBUG -D_FORTIFY_SOURCE=2 -Og -isystem $CONDA_PREFIX/include"

export LDFLAGS="-Wl,-O2 -Wl,--sort-common -Wl,--as-needed -Wl,-z,relro -Wl,-z,now -Wl,--disable-new-dtags -Wl,--gc-sections -Wl,--allow-shlib-undefined -Wl,-rpath,$CONDA_PREFIX/lib -Wl,-rpath-link,$CONDA_PREFIX/lib -L$CONDA_PREFIX/lib"
export ELFEDIT="$CONDA_PREFIX/bin/x86_64-conda-linux-gnu-elfedit"
export LD="$CONDA_PREFIX/bin/x86_64-conda-linux-gnu-ld"
export READELF="$CONDA_PREFIX/bin/x86_64-conda-linux-gnu-readelf"
export SIZE="$CONDA_PREFIX/bin/x86_64-conda-linux-gnu-size"
export AR="$CONDA_PREFIX/bin/x86_64-conda-linux-gnu-ar"
export AS="$CONDA_PREFIX/bin/x86_64-conda-linux-gnu-as"
export LD_GOLD="$CONDA_PREFIX/bin/x86_64-conda-linux-gnu-ld.gold"
export OBJCOPY="$CONDA_PREFIX/bin/x86_64-conda-linux-gnu-objcopy"
export STRIP="$CONDA_PREFIX/bin/x86_64-conda-linux-gnu-strip"
export OBJDUMP="$CONDA_PREFIX/bin/x86_64-conda-linux-gnu-objdump"
export RANLIB="$CONDA_PREFIX/bin/x86_64-conda-linux-gnu-ranlib"
export STRINGS="$CONDA_PREFIX/bin/x86_64-conda-linux-gnu-strings"
