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
#include <vector>


mmap_stream* map_file(const char* filename, AAssetManager* mgr) {
  long pageSize = sysconf(_SC_PAGE_SIZE);
  size_t result;
  off_t start;
  off_t outLength;

  AAsset *asset = AAssetManager_open(mgr, filename, AASSET_MODE_UNKNOWN);
  if(asset == NULL) {
    __android_log_print(ANDROID_LOG_VERBOSE, "MyApp", "Couldn't locate asset [ %s ]", filename);
    return NULL;
  }
  int fd = AAsset_openFileDescriptor(asset, &start, &outLength);
  __android_log_print(ANDROID_LOG_VERBOSE, "MyApp", "Opened file descriptor [ %d ] for [ %s ] with start %lld and length %lld", fd, filename, (long long)start,(long long) outLength);

  off_t startPage = (off_t)(start / pageSize) * pageSize;
  off_t startPageOffset = start - startPage;

  char *mmapped_buffer_ = (char *) mmap(nullptr, outLength + startPageOffset, PROT_READ, MAP_SHARED, fd,
                                       startPage);
  if (mmapped_buffer_ == MAP_FAILED) {
     __android_log_print(ANDROID_LOG_VERBOSE, "MyApp", "mmap failed");
     return NULL;
  }

  char* stream_start = mmapped_buffer_ + startPageOffset;
  char* stream_end = mmapped_buffer_ + startPageOffset + outLength;
  mmap_stream* mmap_buf = new mmap_stream(stream_start, stream_end);
  __android_log_print(ANDROID_LOG_VERBOSE, "MyApp", "Created stream from %p to %p", stream_start, stream_end);

  //AAsset_close(asset);
  return mmap_buf;
}
static istream *mdlstream;
static istream *symbolstream;
static istream *fststream;
static mmap_stream *fstbuf;
static int port_num_actual;

extern "C"
{
    JNIEXPORT jint JNICALL Java_com_example_kaldi_1asr_1ffi_KaldiAsrFfiPlugin_loadFST(JNIEnv *env, jobject obj, jobject assetManager, jstring jfstName) {
        const char *fst_filename_c = env->GetStringUTFChars(jfstName, 0);
        std::string fst_filename(fst_filename_c);
        std::string fst_filepath = "flutter_assets/assets/" + fst_filename;
        AAssetManager *mgr = AAssetManager_fromJava(env, assetManager);

        if(fststream != NULL) { 
          delete(fststream);
          delete(fstbuf);
          fststream = nullptr;
          __android_log_print(ANDROID_LOG_VERBOSE, "MyApp", "Unmapped FST file");
        }
        
        fstbuf = map_file(fst_filepath.c_str(), mgr);
        if(fstbuf == NULL) {
          __android_log_print(ANDROID_LOG_VERBOSE, "MyApp", "Could not mmap FST file %s", fst_filepath.c_str());
          return -1;
        } else {
          __android_log_print(ANDROID_LOG_VERBOSE, "MyApp", "Successfully mmaped FST file %s", fst_filepath.c_str());
        }
        fststream = new istream(fstbuf);

        loadFST(fststream); 
        __android_log_print(ANDROID_LOG_VERBOSE, "MyApp", "FST loaded!");

        env->ReleaseStringUTFChars(jfstName, fst_filename_c);
        return 0;
    }

    JNIEXPORT jint JNICALL Java_com_example_kaldi_1asr_1ffi_KaldiAsrFfiPlugin_initialize(JNIEnv *env, jobject obj, jobject assetManager, jstring jlogFile, jint sampleFrequency) 
    {

        if(mdlstream) {
          __android_log_print(ANDROID_LOG_VERBOSE, "MyApp", "Already initialized.");
          return port_num_actual;
        } 
   
        AAssetManager *mgr = AAssetManager_fromJava(env, assetManager);
        const char *log_filepath_c = env->GetStringUTFChars(jlogFile, 0);
        std::string log_filepath(log_filepath_c);

        mmap_stream* mdlbuf = map_file("flutter_assets/assets/final.mdl", mgr);
        mmap_stream* symbolbuf = map_file("flutter_assets/assets/words_cvte_real.txt", mgr);
        if(mdlbuf == NULL) {
            __android_log_print(ANDROID_LOG_VERBOSE, "MyApp", "Could not map MDL file");
        } else if(symbolbuf== NULL) {
          __android_log_print(ANDROID_LOG_VERBOSE, "MyApp", "Could not map symbol file");
        } 
        mdlstream = new istream(mdlbuf);
        symbolstream = new istream(symbolbuf);
        
        port_num_actual = initialize(mdlstream, symbolstream, (int)sampleFrequency, log_filepath);
        env->ReleaseStringUTFChars(jlogFile, log_filepath_c);
        return port_num_actual;
    }
} 


