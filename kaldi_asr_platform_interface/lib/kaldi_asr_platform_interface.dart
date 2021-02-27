import 'dart:async';
import 'dart:async';
import 'dart:typed_data';
import 'package:plugin_platform_interface/plugin_platform_interface.dart';

enum ConnectionStatus { Connected, Disconnected }

abstract class KaldiAsrPlatform extends PlatformInterface {
  
  KaldiAsrPlatform() : super(token: _token);

  static final Object _token = Object();

  static KaldiAsrPlatform _instance;

  static KaldiAsrPlatform get instance => _instance;

  Stream<String> get decoded;

  Stream<ConnectionStatus> get status;

  static set instance(KaldiAsrPlatform instance) {
    PlatformInterface.verifyToken(instance, _token);
    _instance = instance;
  }

  Future initialize(String fstpath) {
    throw new UnimplementedError("initialize is not implemented");
  }

  void decode(Uint8List data) async {
    throw new UnimplementedError("decode is not implemented");
  }


}
