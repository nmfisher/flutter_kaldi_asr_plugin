import 'dart:async';
import 'dart:async';
import 'package:plugin_platform_interface/plugin_platform_interface.dart';

abstract class KaldiAsrPlatform extends PlatformInterface {
  
  KaldiAsrPlatform() : super(token: _token);

  static final Object _token = Object();

  static KaldiAsrPlatform _instance;

  static KaldiAsrPlatform get instance => _instance;

  static set instance(KaldiAsrPlatform instance) {
    PlatformInterface.verifyToken(instance, _token);
    _instance = instance;
  }

  void initialize(String fstpath) {
    throw new UnimplementedError("initialize is not implemented");
  }

  Future<List<String>> decode(String wav) async {
    throw new UnimplementedError("decode is not implemented");
  }


}
