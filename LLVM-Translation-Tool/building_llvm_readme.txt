Repo clonen:
    git clone --depth=1 https://llvm.org/git/llvm.git
    cd llvm/tools
    git clone --depth=1 https://llvm.org/git/clang.git
    git clone --depth=1 https://llvm.org/git/lld.git
    cd ../projects
    git clone --depth=1 https://llvm.org/git/libcxx.git
    git clone --depth=1 https://llvm.org/git/libcxxabi.git
    git clone --depth=1 https://llvm.org/git/compiler-rt.git
    git clone --depth=1 https://llvm.org/git/openmp.git

llvm bauen:
    mkdir llvm-build  
    cd llvm-build
    cmake -DCMAKE_BUILD_TYPE=DEBUG -DLLVM_TARGETS_TO_BUILD=X86 $LLVM_SRC
    cmake --build .
    #Sehr lange Kaffeepause (ca. 4h)
    cmake -DCMAKE_INSTALL_PREFIX=$LLVM_INSTALL -P cmake_install.cmake


-----------------------------------------------------------------------------------
ninja binary download:
    https://github.com/ninja-build/ninja/releases/download/v1.8.2/ninja-linux.zip

mit nija bauen:
    mkdir llvm-build-ninja
    cd llvm-build-ninja
    cmake -G Ninja -DCMAKE_INSTALL_PREFIX=$LLVM_INSTALL 
    ninja
    #Immernoch lange Kaffeepause (ca. 2h)
    ninja install
    
