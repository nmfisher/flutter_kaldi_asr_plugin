#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/log.h>
#include <jni.h>
#include <fcntl.h>
#include <iostream>
#include <fstream>
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
static DecoderConfiguration configuration;

extern "C"
{

    JNIEXPORT jint JNICALL Java_com_example_kaldi_1asr_1ffi_KaldiAsrFfiPlugin_loadFSTFromFile(JNIEnv *env, jobject obj, jstring jfstFilepath) {
        const char *fst_filepath_c = env->GetStringUTFChars(jfstFilepath, 0);
        if(fststream != NULL) { 
          delete(fststream);
          fststream = nullptr;
          __android_log_print(ANDROID_LOG_VERBOSE, "asrbridge", "Unmapped FST file");
        }
        __android_log_print(ANDROID_LOG_VERBOSE, "asrbridge", "Loading FST from file %s", fst_filepath_c);

        fststream = new ifstream(fst_filepath_c);
        loadFST(fststream); 
        __android_log_print(ANDROID_LOG_VERBOSE, "asrbridge", "FST loaded!");

        env->ReleaseStringUTFChars(jfstFilepath, fst_filepath_c);
        return 0;
    }


    JNIEXPORT jint JNICALL Java_com_example_kaldi_1asr_1ffi_KaldiAsrFfiPlugin_loadFSTFromAsset(JNIEnv *env, jobject obj, jobject assetManager, jstring jfstName) {
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

    JNIEXPORT jintArray JNICALL Java_com_example_kaldi_1asr_1ffi_KaldiAsrFfiPlugin_initialize(JNIEnv *env, jobject obj, jobject assetManager, jstring jlogFile, jint sampleFrequency) 
    {

        if(!mdlstream) {
            
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
          
          configuration = initialize(mdlstream, symbolstream, (int)sampleFrequency, log_filepath);

          env->ReleaseStringUTFChars(jlogFile, log_filepath_c);
          __android_log_print(ANDROID_LOG_VERBOSE, "asrbridge", "Released logfile string");
        } else {
          __android_log_print(ANDROID_LOG_VERBOSE, "asrbridge", "Already initialized.");
        }
        jintArray result;
        result = env->NewIntArray(2);
        if (result == NULL) {
           return NULL; /* out of memory error thrown */
        }
        jint data[2] = {configuration.input_port, configuration.output_port };
        env->SetIntArrayRegion(result, 0, 2, data);
        return result;
    }
} 


