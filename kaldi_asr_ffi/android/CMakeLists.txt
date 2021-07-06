cmake_minimum_required(VERSION 3.4.1)  # for example

include_directories(src/main/c/include)
include_directories(src/main/c/src)
link_directories(src/main/jniLibs/${ANDROID_ABI})
add_library( 
            asrbridge
            SHARED
            src/main/c/src/bridge/asrbridge_online.cc
            src/main/c/src/bridge/asrbridge_online_android.cc
            src/main/c/src/mmap/mmap_stream.cpp 
            src/main/c/src/server/server.cc 
        )


target_link_libraries(asrbridge -landroid -llog -lfst -ldl -lkaldi-fstext -lkaldi-nnet3 -lkaldi-online2 
-lkaldi-feat -lkaldi-ivector -lkaldi-lm -lkaldi-decoder -lkaldi-lat 
-lkaldi-hmm -lkaldi-transform -lkaldi-gmm -lkaldi-tree -lkaldi-cudamatrix 
-lkaldi-matrix -lkaldi-util -lkaldi-base)
