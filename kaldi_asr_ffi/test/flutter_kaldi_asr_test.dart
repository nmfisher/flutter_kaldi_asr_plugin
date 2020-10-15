import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:kaldi_asr_ffi/kaldi_asr_ffi.dart';

void main() {
  const MethodChannel channel = MethodChannel('kaldi_asr_ffi');

  TestWidgetsFlutterBinding.ensureInitialized();

  setUp(() {
    channel.setMockMethodCallHandler((MethodCall methodCall) async {
      return '42';
    });
  });

  tearDown(() {
    channel.setMockMethodCallHandler(null);
  });

  test('getPlatformVersion', () async {
    expect(await FlutterKaldiAsr.platformVersion, '42');
  });
}
