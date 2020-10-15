#import "KaldiAsrFfiPlugin.h"
#if __has_include(<kaldi_asr_ffi/kaldi_asr_ffi-Swift.h>)
#import <kaldi_asr_ffi/kaldi_asr_ffi-Swift.h>
#else
// Support project import fallback if the generated compatibility header
// is not copied when this plugin is created as a library.
// https://forums.swift.org/t/swift-static-libraries-dont-copy-generated-objective-c-header/19816
#import "kaldi_asr_ffi-Swift.h"
#endif

@implementation KaldiAsrFfiPlugin
+ (void)registerWithRegistrar:(NSObject<FlutterPluginRegistrar>*)registrar {
  [SwiftKaldiAsrFfiPlugin registerWithRegistrar:registrar];
}
@end
