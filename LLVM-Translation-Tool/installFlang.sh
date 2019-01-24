#!/usr/bin/env bash

[ "$DEBUG" == 'true' ] && set -x
set -e
set -u

#Set env-Variables
CC=gcc
CXX=g++
CXXFLAGS="-std=c++17"
LDFLAGS=
INCFLAGS=

FLANG_BUILD=`pwd`/flang_build
SOURCEDIR_FLANG=`pwd`/sources/flang/
INSTALLDIR=`pwd`/flang_install/
INSTALL_LOG=`pwd`/log_flang_install.log
PATCHES_DIR=`pwd`/sources/patches/

touch $INSTALL_LOG

#apply patches
PATCH=${PATCH=false}

echo "Set options: "
echo "Apply Flang-patches: $PATCH"
echo

sleep 2s

if [ "$PATCH" = true ]; then
    cd $PATCHES_DIR
    #llvm patch
    if [ ! -f 56.diff ]; then
        wget https://github.com/llvm-mirror/llvm/pull/56.diff
    fi
    #clang patch
    if [ ! -f 40.diff ]; then
        wget https://github.com/llvm-mirror/clang/pull/40.diff
    fi

    cd $SOURCEDIR_FLANG
    git checkout release_60
    git apply $PATCHES_DIR/56.diff -v | tee --append $INSTALL_LOG ; test ${PIPESTATUS[0]} -eq 0
    cd tools/clang
    git checkout release_60
    git apply $PATCHES_DIR/40.diff -v | tee --append $INSTALL_LOG ; test ${PIPESTATUS[0]} -eq 0

    echo "Patches have been applied" | tee --append $INSTALL_LOG
fi


#load necessary spack modules
module purge || true
spack load gcc || true
spack load cmake || true

if [[ -d $FLANG_BUILD ]]; then
    while true; do
        read -p "$FLANG_BUILD already exists. Shall it be deleted? (y/n) " yn
        case $yn in
            [Yy]* ) rm -rf $FLANG_BUILD; mkdir -p $FLANG_BUILD; break;;
            [Nn]* ) break;;
        esac
    done
else
    mkdir -p $FLANG_BUILD
fi

cd $FLANG_BUILD

#build llvm
echo "Build LLVM" | tee --append $INSTALL_LOG

CC=gcc
CXX=g++
CFLAGS="std=c17"
CXXFLAGS="-std=c++17"
LDFLAGS=
INCFLAGS=

#build llvm
cmake $SOURCEDIR_FLANG -G "Unix Makefiles" \
    -DCMAKE_CXX_STANDARD=17 \
    -DCLANG_DEFAULT_CXX_STDLIB=libstdc++ \
    -DLLVM_TARGETS_TO_BUILD=X86 \
    -DC_INCLUDE_DIRS=$INCFLAGS\
    -DLLVM_ENABLE_WERROR=OFF \
    -DCMAKE_BUILD_TYPE=DEBUG \
    -DCMAKE_C_COMPILER=$CC \
    -DCMAKE_CXX_COMPILER=$CXX \
    -DCMAKE_EXE_LINKER_FLAGS=$LDFLAGS \
    -DCMAKE_CXX_FLAGS_RELEASE=$CXXFLAGS \
    -DCMAKE_C_FLAGS_RELEASE=$CFLAGS \
    -DCMAKE_INSTALL_PREFIX=$INSTALLDIR \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

CPUS=$(nproc)

echo -e "\n\n\nBuild with $CPUS CPU cores\n" | tee --append $INSTALL_LOG
make -j ${CPUS} VERBOSE=1 2>&1 | tee --append $INSTALL_LOG; test ${PIPESTATUS[0]} -eq 0

echo -e "\n\n\nRun checks\n" | tee --append $INSTALL_LOG
make -j ${CPUS} check-llvm VERBOSE=1 2>&1 | tee --append $INSTALL_LOG ; test ${PIPESTATUS[0]} -eq 0

echo -e "\n\n\nInstall\n" | tee --append $INSTALL_LOG
make install VERBOSE=1 2>&1 | tee --append $INSTALL_LOG ; test ${PIPESTATUS[0]} -eq 0

echo -e "\n\n\nInstallation has been finished\n" | tee --append $INSTALL_LOG

