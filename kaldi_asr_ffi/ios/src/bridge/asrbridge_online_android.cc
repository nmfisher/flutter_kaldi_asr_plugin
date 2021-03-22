#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/log.h>
#include <android/log.h>
#include <jni.h>
#include <fcntl.h>
#include <iostream>
#include <sstream>
#include <sys/types.h> 
#include <sys/stat.h> 
#include <sys/mman.h>
#include <unistd.h>
#include "mmap/mmap_stream.hpp"
#include "asrbridge_online.hpp"
#include "mmap/mmap_stream_android.hpp"
#include <vector>


static istream *mdlstream;
static istream *symbolstream;
static istream *fststream;
static mmap_stream *fstbuf;
static int port_num_actual;

extern "C"
{
    JNIEXPORT jint JNICALL Java_com_example_kaldi_1asr_1ffi_KaldiAsrFfiPlugin_loadFST(JNIEnv *env, jobject obj, jobject assetManager, jstring jfstName) {
        const char *fst_assetpath_c = env->GetStringUTFChars(jfstName, 0);
        std::string fst_assetpath(fst_assetpath_c);
        std::string fst_filepath = "flutter_assets/" + fst_assetpath;
        AAssetManager *mgr = AAssetManager_fromJava(env, assetManager);

        if(fststream != NULL) { 
          delete(fststream);
          delete(fstbuf);
          fststream = nullptr;
          __android_log_print(ANDROID_LOG_VERBOSE, "asrbridge", "Unmapped FST file");
        }
        
        fstbuf = map_file(fst_filepath.c_str(), mgr);
        if(fstbuf == NULL) {
          __android_log_print(ANDROID_LOG_VERBOSE, "asrbridge", "Could not mmap FST file %s", fst_filepath.c_str());
          return -1;
        } else {
          __android_log_print(ANDROID_LOG_VERBOSE, "asrbridge", "Successfully mmaped FST file %s", fst_filepath.c_str());
        }
        fststream = new istream(fstbuf);

        loadFST(fststream); 
        __android_log_print(ANDROID_LOG_VERBOSE, "asrbridge", "FST loaded!");

        env->ReleaseStringUTFChars(jfstName, fst_assetpath_c);
        return 0;
    }

    JNIEXPORT jint JNICALL Java_com_example_kaldi_1asr_1ffi_KaldiAsrFfiPlugin_initialize(JNIEnv *env, jobject obj, jobject assetManager, jstring jlogFile, jint sampleFrequency) 
    {

        if(mdlstream) {
          __android_log_print(ANDROID_LOG_VERBOSE, "asrbridge", "Already initialized.");
          return port_num_actual;
        } 
   
        AAssetManager *mgr = AAssetManager_fromJava(env, assetManager);
        const char *log_filepath_c = env->GetStringUTFChars(jlogFile, 0);
        std::string log_filepath(log_filepath_c);

        mmap_stream* mdlbuf = map_file("flutter_assets/assets/final.mdl", mgr);
        mmap_stream* symbolbuf = map_file("flutter_assets/assets/words_cvte_real.txt", mgr);
        if(mdlbuf == NULL) {
            __android_log_print(ANDROID_LOG_VERBOSE, "asrbridge", "Could not map MDL file");
        } else if(symbolbuf== NULL) {
          __android_log_print(ANDROID_LOG_VERBOSE, "asrbridge", "Could not map symbol file");
        } 
        mdlstream = new istream(mdlbuf);
        symbolstream = new istream(symbolbuf);
        
        port_num_actual = initialize(mdlstream, symbolstream, (int)sampleFrequency, log_filepath);

        __android_log_print(ANDROID_LOG_VERBOSE, "asrbridge", "Created decoder on %d", port_num_actual);

        env->ReleaseStringUTFChars(jlogFile, log_filepath_c);
        __android_log_print(ANDROID_LOG_VERBOSE, "asrbridge", "Released logfile string");
        return port_num_actual;
    }
} 


