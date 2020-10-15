#
# To learn more about a Podspec see http://guides.cocoapods.org/syntax/podspec.html.
# Run `pod lib lint kaldi_asr_ffi.podspec' to validate before publishing.
#
Pod::Spec.new do |s|
  s.name             = 'kaldi_asr_ffi'
  s.version          = '0.0.1'
  s.summary          = 'A new flutter plugin project.'
  s.description      = <<-DESC
A new flutter plugin project.
                       DESC
  s.homepage         = 'http://example.com'
  s.license          = { :file => '../LICENSE' }
  s.author           = { 'Your Company' => 'email@example.com' }
  s.source           = { :path => '.' }
  s.source_files = 'Classes/**/*'
  s.dependency 'Flutter'
  s.platform = :ios, '8.0'
  s.vendored_libraries="**/*.a"
  # Flutter.framework does not contain a i386 slice. Only x86_64 simulators are supported.
  s.pod_target_xcconfig = { 'DEFINES_MODULE' => 'YES', 'VALID_ARCHS[sdk=iphonesimulator*]' => 'x86_64' }
  s.swift_version = '5.0'
  s.xcconfig = { 'OTHER_LDFLAGS' => '-framework Accelerate -lstdc++  -force_load "${PODS_ROOT}/../.symlinks/plugins/kaldi_asr_ffi/ios/lib/libfst.a" -force_load "${PODS_ROOT}/../.symlinks/plugins/kaldi_asr_ffi/ios/lib/libkaldi-base.a" -force_load "${PODS_ROOT}/../.symlinks/plugins/kaldi_asr_ffi/ios/lib/libkaldi-decoder.a" -force_load "${PODS_ROOT}/../.symlinks/plugins/kaldi_asr_ffi/ios/lib/libkaldi-gmm.a" -force_load "${PODS_ROOT}/../.symlinks/plugins/kaldi_asr_ffi/ios/lib/libkaldi-kws.a" -force_load "${PODS_ROOT}/../.symlinks/plugins/kaldi_asr_ffi/ios/lib/libkaldi-matrix.a" -force_load "${PODS_ROOT}/../.symlinks/plugins/kaldi_asr_ffi/ios/lib/libkaldi-nnet3.a" -force_load "${PODS_ROOT}/../.symlinks/plugins/kaldi_asr_ffi/ios/lib/libkaldi-sgmm2.a" -force_load "${PODS_ROOT}/../.symlinks/plugins/kaldi_asr_ffi/ios/lib/libkaldi-util.a" -force_load "${PODS_ROOT}/../.symlinks/plugins/kaldi_asr_ffi/ios/lib/libkaldi-chain.a" -force_load "${PODS_ROOT}/../.symlinks/plugins/kaldi_asr_ffi/ios/lib/libkaldi-feat.a" -force_load "${PODS_ROOT}/../.symlinks/plugins/kaldi_asr_ffi/ios/lib/libkaldi-hmm.a" -force_load "${PODS_ROOT}/../.symlinks/plugins/kaldi_asr_ffi/ios/lib/libkaldi-lat.a" -force_load "${PODS_ROOT}/../.symlinks/plugins/kaldi_asr_ffi/ios/lib/libkaldi-nnet.a" -force_load "${PODS_ROOT}/../.symlinks/plugins/kaldi_asr_ffi/ios/lib/libkaldi-online2.a" -force_load "${PODS_ROOT}/../.symlinks/plugins/kaldi_asr_ffi/ios/lib/libkaldi-transform.a" -force_load "${PODS_ROOT}/../.symlinks/plugins/kaldi_asr_ffi/ios/lib/libasrbridge.a" -force_load "${PODS_ROOT}/../.symlinks/plugins/kaldi_asr_ffi/ios/lib/libkaldi-cudamatrix.a" -force_load "${PODS_ROOT}/../.symlinks/plugins/kaldi_asr_ffi/ios/lib/libkaldi-fstext.a" -force_load "${PODS_ROOT}/../.symlinks/plugins/kaldi_asr_ffi/ios/lib/libkaldi-ivector.a" -force_load "${PODS_ROOT}/../.symlinks/plugins/kaldi_asr_ffi/ios/lib/libkaldi-lm.a" -force_load "${PODS_ROOT}/../.symlinks/plugins/kaldi_asr_ffi/ios/lib/libkaldi-nnet2.a" -force_load "${PODS_ROOT}/../.symlinks/plugins/kaldi_asr_ffi/ios/lib/libkaldi-rnnlm.a" -force_load "${PODS_ROOT}/../.symlinks/plugins/kaldi_asr_ffi/ios/lib/libkaldi-tree.a" '}
end
