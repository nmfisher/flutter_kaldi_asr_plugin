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

static ifstream* mdlStream;
static ifstream* symbolStream;
static ifstream* fstStream;
static const char* mdlStreamPath;
static const char* symbolStreamPath;

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

      NSLog(@"Loading model from asset path : %@", path);

      mdlStreamPath = [path UTF8String];
      mdlStream = new ifstream(mdlStreamPath);
      key = [registrar lookupKeyForAsset:@"assets/words_cvte_real.txt"];
      path = [[NSBundle mainBundle] pathForResource:key ofType:nil];
      symbolStreamPath = [path UTF8String];
      symbolStream = new ifstream(symbolStreamPath);
      DecoderConfiguration config = initialize(mdlStream, symbolStream, sampleFrequency, logFileStr);
      NSNumber* input_port_num_ns = [NSNumber numberWithInt:config.input_port];
      NSNumber* output_port_num_ns = [NSNumber numberWithInt:config.output_port];
      NSLog(@"Initialized decoder with input port num : %@ and output port %@", input_port_num_ns, output_port_num_ns);
        NSArray *port_nums = @[ input_port_num_ns, output_port_num_ns ];
      result(port_nums);
    } else if([@"loadFSTFromAsset" isEqualToString:call.method]) {
        NSString* fstPath = [NSString stringWithFormat: @"%@", call.arguments[@"fst"]];
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
        NSLog(@"LOADED FST PATH %@", path);
        result(0);
    } else if([@"loadFSTFromFile" isEqualToString:call.method]) {
        NSString* fstPath = call.arguments[@"fst"];
        if(fstStream) {
          delete(fstStream);	
        }
        fstStream = new ifstream([fstPath UTF8String]);
        if(!fstStream) {
            NSLog(@"Error loading FST stream");
            result([NSNumber numberWithInt:-1]);
            return;
        }
        loadFST(fstStream);
        NSLog(@"LOADED FST PATH %@", fstPath);
        result(0);
    } else {
      NSLog(@"Invalid method : %@", call.method);
      result(FlutterMethodNotImplemented);
    } 
    return;
  }];
}
@end

