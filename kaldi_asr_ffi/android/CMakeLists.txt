cmake_minimum_required(VERSION 3.4.1)  # for example
set(CMAKE_BUILD_TYPE DEBUG)
set(CMAKE_C_FLAGS "-O0 -ggdb")
set(CMAKE_C_FLAGS_DEBUG "-O0 -ggdb")
set(CMAKE_C_FLAGS_RELEASE "-O0 -ggdb")
set(CMAKE_CXX_FLAGS "-O0 -ggdb")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -ggdb")
set(CMAKE_CXX_FLAGS_RELEASE "-O0 -ggdb")

include_directories(../ios/src)
include_directories(../ios/include)
link_directories(src/main/jniLibs/${ANDROID_ABI})

string(REPLACE "build\\kaldi_asr_ffi" "build\\flutter_mmap_stream" FLUTTER_MMAP_BUILD_DIR ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
link_directories(${FLUTTER_MMAP_BUILD_DIR})

add_library( 
            asrbridge
            SHARED
            ../ios/src/bridge/asrbridge_online.cc
            ../ios/src/bridge/asrbridge_online_android.cc
            ../ios/src/server/server.cc 
        )


target_link_libraries(asrbridge -lmmap_stream_android -landroid -llog -lfst -ldl -lkaldi-fstext -lkaldi-nnet3 -lkaldi-online2 
-lkaldi-feat -lkaldi-ivector -lkaldi-lm -lkaldi-decoder -lkaldi-lat 
-lkaldi-hmm -lkaldi-transform -lkaldi-gmm -lkaldi-tree -lkaldi-cudamatrix 
-lkaldi-matrix -lkaldi-util -lkaldi-base)
