#!/usr/bin/env bash


[ "$DEBUG" == 'true' ] && set -x
set -e
set -u


CLONE=${CLONE=false}
POLLY=${POLLY=false}
DEPTH=${DEPTH=false}
FLANG=${FLANG=false}
PATCH=${PATCH=false}
CLONEOPTIONS=""
[ "$DEPTH" = true ] && CLONEOPTIONS="$CLONEOPTIONS --depth=1"

echo "Set env-variables: "
echo "CLONE:         $CLONE"
echo "POLLY:         $POLLY"
echo "DEPTH:         $DEPTH"
echo "FLANG:         $FLANG"
echo "PATCH:         $PATCH"
echo "CLONEOPTIONS:  $CLONEOPTIONS"
echo
sleep 2s

SRC=`pwd`/sources
LLVM_SRC=${SRC}/llvm
TOOLS_SRC=${LLVM_SRC}/tools
POLLY_SRC=${TOOLS_SRC}/polly
CLANG_SRC=${TOOLS_SRC}/clang
FLANG_SRC=${SRC}/flang
PROJECTS_SRC=${LLVM_SRC}/projects
PATCHES_DIR=`pwd`/sources/patches/


if [ "$CLONE" == 'true' ]; then

    echo "Delete sources and clone repository"
    sleep 5s
    rm -rf $LLVM_SRC

    #LLVM
    echo -e "\nClone LLVM"
    git clone $CLONEOPTIONS http://llvm.org/git/llvm.git $LLVM_SRC

    #CLANG
    echo -e "\nClone CLANG"
    git clone $CLONEOPTIONS http://llvm.org/git/clang.git ${CLANG_SRC}

    #Polly
    if [ "$POLLY" == 'true' ]; then
        echo -e "\nClone Polly"
        #git clone --depth=1 http://llvm.org/git/polly.git $POLLY_SRC
        git clone $CLONEOPTIONS http://llvm.org/git/polly.git $POLLY_SRC
    fi

    #compiler-rt
    echo -e "\nClone compiler-rt"
    git clone $CLONEOPTIONS http://llvm.org/git/compiler-rt.git ${PROJECTS_SRC}/compiler-rt

    #OpenMP-Support
    echo -e "\nClone OpenMP support"
    git clone $CLONEOPTIONS http://llvm.org/git/openmp.git ${PROJECTS_SRC}/openmp

    #ĺibcxx, libcxxabi
    echo -e "\nClone libcxx and libcxxabi"
    git clone $CLONEOPTIONS http://llvm.org/git/libcxx.git ${PROJECTS_SRC}/libcxx
    git clone $CLONEOPTIONS http://llvm.org/git/libcxxabi.git ${PROJECTS_SRC}/libcxxabi

    #Test Suite
    echo -e "\nClone test suite"
    git clone $CLONEOPTIONS http://llvm.org/git/test-suite.git ${PROJECTS_SRC}/test-suite

    if [ "$FLANG" == true ]; then
        echo -e "\nClone Flang-Compiler"
        git clone $CLONEOPTIONS https://github.com/flang-compiler/flang.git $FLANG_SRC
    fi

   #apply patches
    if [ "$PATCH" = true ]; then
        echo -e "\nApply Patches"
        mkdir -p $PATCHES_DIR
        cd $PATCHES_DIR
        #llvm patch
        if [ ! -f 56.diff ]; then
            wget https://github.com/llvm-mirror/llvm/pull/56.diff
        fi
        #clang patch
        if [ ! -f 40.diff ]; then
            wget https://github.com/llvm-mirror/clang/pull/40.diff
        fi

        #clang patch
        if [ ! -f 35.diff ]; then
            wget https://github.com/llvm-mirror/clang/pull/40.diff
        fi

        cd $LLVM_SRC
#        git checkout release_60
#        git apply $PATCHES_DIR/56.diff -v | tee --append $INSTALL_LOG #; test ${PIPESTATUS[0]} -eq 0
#        cd tools/clang
#        git checkout release_60
#        git apply $PATCHES_DIR/40.diff -v | tee --append $INSTALL_LOG #; test ${PIPESTATUS[0]} -eq 0
        git checkout release_50
        git apply $PATCHES_DIR/35.diff -v | tee --append $INSTALL_LOG #; test ${PIPESTATUS[0]} -eq 0

        echo "Patches have been applied" | tee --append $INSTALL_LOG
    fi

else

    #LLVM
    echo "Pull LLVM"
    git -C $LLVM_SRC pull --rebase
    #CLANG
    echo "Pull Clang"
    git -C $CLANG_SRC pull --rebase

    #Polly
    echo "Pull Polly"
    git -C $POLLY_SRC pull --rebase

    #compiler-rt
    echo "Pull compiler-rt"
    git -C ${PROJECTS_SRC}/compiler-rt pull --rebase

    #OpenMP-Support
    echo "Pull openmp"
    git -C ${PROJECTS_SRC}/openmp pull --rebase
    #ĺibcxx
    echo "Pull libcxx"
    git -C ${PROJECTS_SRC}/libcxx pull --rebase

    #libcxxabi
    echo "Pull libcxxabi"
    git -C ${PROJECTS_SRC}/libcxxabi pull --rebase

    #Test Suite
    echo "Pull test-suite"
    git -C ${PROJECTS_SRC}/test-suite pull --rebase

    #Sonderbehandlung für Flang, da Patches für Release_60 + pull Probleme bereiten könnte
    if [ "$FLANG" == true ]; then
        #Flang
        echo "Pull flang"
        git -C ${FLANG_SRC} --rebase
    fi
fi
