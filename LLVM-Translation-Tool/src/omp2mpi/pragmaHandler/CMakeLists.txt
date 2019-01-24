add_library(omp2mpiClangPlugin MODULE
	#List your source files here.
    PragmaHandlerPlugin.cpp   
)

#Use c++ 17 now
target_compile_features(omp2mpiClangPlugin PRIVATE cxx_range_for cxx_auto_type cxx_std_17)

#LLVM is(typically) built with no C++ RTTI.We need to match that;
#otherwise, we'll get linker errors about missing RTTI data.
set_target_properties(omp2mpiClangPlugin PROPERTIES
	COMPILE_FLAGS "-O3 -Wall -Wextra -fno-rtti -Wno-unused-parameter -Wno-comment -Wno-strict-aliasing"
)
#Wcomment and Wstrict-aliasing originates from imported clang source and are therefore switched off