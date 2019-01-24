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

LLVM_BUILD=`pwd`/llvm_build
SOURCEDIR_LLVM=`pwd`/sources/llvm/
INSTALLDIR_FINAL=`pwd`/llvm_install/
INSTALLDIR_TMP=`pwd`/tmp_llvm_install/
INSTALL_LOG=`pwd`/log_llvm_install.log

touch $INSTALL_LOG

RECOMPILE=${RECOMPILE=false}

echo "Set options: "
echo "RECOMPILE (with clang): $RECOMPILE"
echo
sleep 2s


if [ "$RECOMPILE" = true ]; then
    INSTALLDIR=$INSTALLDIR_TMP
else
    INSTALLDIR=$INSTALLDIR_FINAL
fi

#load necessary spack modules
module purge || true
spack load gcc || true
spack load cmake || true

if [[ -d $LLVM_BUILD ]]; then
    while true; do
        read -p "$LLVM_BUILD already exists. Shall it be deleted? (y/n) " yn
        case $yn in
            [Yy]* ) rm -rf $LLVM_BUILD; mkdir -p $LLVM_BUILD; break;;
            [Nn]* ) break;;
        esac
    done
else
    mkdir -p $LLVM_BUILD
fi

cd $LLVM_BUILD

#build llvm
echo "Build LLVM" | tee --append $INSTALL_LOG

CC=gcc
CXX=g++
CFLAGS="-std=c17"
CXXFLAGS="-std=c++17"
LDFLAGS=
INCFLAGS=

BUILT_WITH_CLANG=false
while true; do
    #build llvm
    cmake $SOURCEDIR_LLVM -G "Unix Makefiles" \
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
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON |tee --append $INSTALL_LOG

    CPUS=$(nproc)
    echo -e "\n\n\nBuild with $CPUS CPU cores\n" | tee --append $INSTALL_LOG
    make -j ${CPUS} VERBOSE=1 2>&1 | tee --append $INSTALL_LOG; test ${PIPESTATUS[0]} -eq 0

    echo -e "\n\n\nRun checks\n" | tee --append $INSTALL_LOG
    if [ ! "$BUILT_WITH_CLANG" = true ]; then
        make -j ${CPUS} check-llvm VERBOSE=1 2>&1 | tee --append $INSTALL_LOG #; test ${PIPESTATUS[0]} -eq 0
    else
        #some analysis tests fail
        make -j ${CPUS} check-all VERBOSE=1 2>&1 | tee --append $INSTALL_LOG #; test ${PIPESTATUS[0]} -eq 0
    fi

    echo -e "\n\n\nInstall\n" | tee --append $INSTALL_LOG
    make install VERBOSE=1 2>&1 | tee --append $INSTALL_LOG ; test ${PIPESTATUS[0]} -eq 0

    if [ "$RECOMPILE" = true ]; then
        echo -e "\n\n\nInitial installation has been finished\n" | tee --append $INSTALL_LOG
        echo -e "\n\n\nRebuild LLVM with clang\n" | tee --append $INSTALL_LOG
        rm -rf $LLVM_BUILD
        mkdir -p $LLVM_BUILD
        cd $LLVM_BUILD

        INSTALLDIR=$INSTALLDIR_FINAL
        CC=$INSTALLDIR_TMP/bin/clang
        CXX=$INSTALLDIR_TMP/bin/clang++

        RECOMPILE=false
        BUILT_WITH_CLANG=true
    else
        echo -e "\n\n\nInstallation has been finished\n" | tee --append $INSTALL_LOG
        break
    fi
done
