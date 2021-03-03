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

#import "fstr"
#import "asrbridge_online.h"

ifstream* mdlStream;
ifstream* symbolStream;

@implementation KaldiAsrFfiPlugin
+ (void)registerWithRegistrar:(NSObject<FlutterPluginRegistrar>*)registrar {
  FlutterMethodChannel* channel =
      [FlutterMethodChannel methodChannelWithName:@"com.avinium/kaldi_asr_ffi"
                                  binaryMessenger:[registrar messenger]];

  [channel setMethodCallHandler:^(FlutterMethodCall* call, FlutterResult result) {
    if ([@"initialize" isEqualToString:call.method]) {
      NSString* logFile = call.arguments[@"logFile"]
      

      NSString* key = [registrar lookupKeyForAsset:@"flutter_assets/assets/final.mdl"];
      NSString* path = [[NSBundle mainBundle] pathForResource:key ofType:nil];
      mdlStream = new ifstream(path);
      key = [registrar lookupKeyForAsset:@"flutter_assets/assets/words_cvte_real.txt"];
      path = [[NSBundle mainBundle] pathForResource:key ofType:nil];
      symbolStream = new ifstream(path);
                                     
    } else if([@"initialize" isEqualToString:call.method]) {
      NSString* fstPath = [NSString stringWithFormat: @"flutter_assets/assets/%@", call.arguments[@"fstPath"]];

    } else {
      NSLog(@"Invalid method : %@", call.method);
      result(FlutterMethodNotImplemented);
    } 
    return;
  }];
}
@end

