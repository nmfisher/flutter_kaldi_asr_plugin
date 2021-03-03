#import "KaldiAsrFfiPlugin.h"
#if __has_include(<kaldi_asr_ffi/kaldi_asr_ffi-Swift.h>)
#import <kaldi_asr_ffi/kaldi_asr_ffi-Swift.h>
#else
// Support project import fallback if the generated compatibility header
// is not copied when this plugin is created as a library.
// https://forums.swift.org/t/swift-static-libraries-dont-copy-generated-objective-c-header/19816
#import "kaldi_asr_ffi-Swift.h"
#endif

#include <iostream>
#include <fstream>

#import "bridge/asrbridge_online.hpp"
using namespace std;

ifstream* mdlStream;
ifstream* symbolStream;
ifstream* fstStream;

@implementation KaldiAsrFfiPlugin
+ (void)registerWithRegistrar:(NSObject<FlutterPluginRegistrar>*)registrar {
  FlutterMethodChannel* channel =
      [FlutterMethodChannel methodChannelWithName:@"com.avinium.kaldi_asr_ffi"
                                  binaryMessenger:[registrar messenger]];

  [channel setMethodCallHandler:^(FlutterMethodCall* call, FlutterResult result) {
    if ([@"initialize" isEqualToString:call.method]) {
     

      NSString* logFile = call.arguments[@"log"];
      string logFileStr = string([logFile UTF8String]);
      int sampleFrequency = [call.arguments[@"sampleFrequency"] intValue];
      NSString* key = [registrar lookupKeyForAsset:@"assets/final.mdl"];
      NSString* path = [[NSBundle mainBundle] pathForResource:key ofType:nil];
      

      mdlStream = new ifstream([path UTF8String]);
      key = [registrar lookupKeyForAsset:@"assets/words_cvte_real.txt"];
      path = [[NSBundle mainBundle] pathForResource:key ofType:nil];
      symbolStream = new ifstream([path UTF8String]);
      int port_num = initialize(mdlStream, symbolStream, sampleFrequency, logFileStr);
      NSNumber* port_num_ns = [NSNumber numberWithInt:port_num];
      NSLog(@"Initialized decoder with port num : %@", port_num_ns);
      result(port_num_ns);
    } else if([@"loadFST" isEqualToString:call.method]) {
        NSString* fstPath = [NSString stringWithFormat: @"assets/%@", call.arguments[@"fst"]];
        NSString* key = [registrar lookupKeyForAsset:fstPath];
        NSString* path = [[NSBundle mainBundle] pathForResource:key ofType:nil];

        if(fstStream) {
          delete(fstStream);	
        }
        
        NSLog(@"LOADING FST FROM PATH %@", path);

        fstStream = new ifstream([path UTF8String]);
        if(!fstStream) {
            NSLog(@"Error loading FST stream");
            result([NSNumber numberWithInt:-1]);
            return;
        }
        loadFST(fstStream);
    } else {
      NSLog(@"Invalid method : %@", call.method);
      result(FlutterMethodNotImplemented);
    } 
    return;
  }];
}
@end

