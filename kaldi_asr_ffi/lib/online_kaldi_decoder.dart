import 'package:flutter/services.dart';
import 'dart:async';
import 'dart:convert';
import 'dart:io';
import 'dart:typed_data';
import 'package:kaldi_asr_platform_interface/kaldi_asr_platform_interface.dart';
import 'dart:ffi';
import 'package:path/path.dart';

class DecoderConfiguration extends Struct {
  @Int32()
  external int get inputPort;
  external set inputPort(int value);

  @Int32()
  external int get outputPort;
  external set outputPort(int value);
}

class OnlineKaldiDecoder extends KaldiAsrPlatform {
  bool debug;

  static const MethodChannel _channel =
      const MethodChannel('com.avinium.kaldi_asr_ffi');

  late String _logDir;
  late double _sampFreq;

  Future<Socket>? _socket;
  late int _inputPortNum;
  late int _outputPortNum;
  StreamSubscription? _listener;

  Stream<String> get decoded => _decodedController.stream;
  StreamController<String> _decodedController =
      StreamController<String>.broadcast();

  Stream<ConnectionStatus> get status => _statusController.stream;
  StreamController<ConnectionStatus> _statusController =
      StreamController<ConnectionStatus>();

  /// Constructs an uninitialized plugin instance to perform online decoding.
  /// [_logDir] is the path of the directory where a log file will be written.
  /// [_sampFreq] is the sample frequency of the input audio.
  /// This constructor does not actually initialize the decoder, so once an instance is created, [initialize()] must be called before the decoder is available.
  OnlineKaldiDecoder(this._logDir, this._sampFreq, {this.debug = true});

  void _debug(String message) {
    if (debug) print(message);
  }

  ///
  /// Load a FST from the specified (Flutter asset path.
  ///
  Future loadFSTFromAsset(String fstFilename) async {
    print("Loading FST with filename $fstFilename");
    var retCode =
        await _channel.invokeMethod('loadFSTFromAsset', {"fst": fstFilename});
    print("FST load result : $retCode");
  }

  ///
  /// Load a FST from the specified filepath.
  ///
  Future loadFSTFromFile(String fstFilepath) async {
    var retCode =
        await _channel.invokeMethod('loadFSTFromFile', {"fst": fstFilepath});
    print("FST load result : $retCode");
  }

  ///
  /// Initializes this plugin instance, creating a decoder that listens on two TCP ports - one that accepts audio data for input, and another on which output will be written.
  /// This method must be called prior to [loadFSTFromFile] or [loadFSTFromAsset], and is safe to call a number of times.
  /// Returns a list of two integers, representing the input/output ports.
  ///
  Future<List<int>> initialize() async {
    var logfile = join(_logDir, "log");
    _debug("Writing to $logfile");

    var portNumbers = (await _channel.invokeMethod<List<Object?>>(
            'initialize', {"log": logfile, "sampleFrequency": _sampFreq}))!
        .cast<int>();
    if (portNumbers.length != 2 || portNumbers[0] < 0 || portNumbers[1] < 0)
      throw Exception(
          "Unknown error initializing Kaldi plugin. Check log for further details");
    _inputPortNum = portNumbers[0];
    _outputPortNum = portNumbers[1];
    _debug(
        "Decoder successfully configured, listening on port $_inputPortNum for input, output will be available on port $_outputPortNum");
    return portNumbers;
  }

  ///
  /// Connect to the decoder output socket.
  ///
  Future connect() async {
    print("Connecting to decoder socket");
    if ((await _socket) != null) return;

    try {
      _listener?.cancel();
      _socket = Socket.connect(InternetAddress.loopbackIPv4, _outputPortNum,
          timeout: Duration(seconds: 30));
      print("Connected to output decoder socket on port $_outputPortNum");

      _listener = (await _socket!).listen((data) async {
        var decoded = utf8.decode(data);
        _decodedController.add(decoded);
      });
      _statusController.add(ConnectionStatus.Connected);
    } catch (err) {
      print("Error connecting to socket : $err");
    } finally {}
  }

  bool disconnecting = false;

  ///
  /// Disconnect from the remote online decoder socket.
  ///
  Future disconnect() async {
    print("Disconnecting");
    if (_socket == null) {
      return;
    }
    disconnecting = true;
    try {
      await (await _socket!).flush();
    } catch (err) {
      print("Error flushing socket : $err");
    } finally {
      try {
        await (await _socket!).close();
      } catch (err) {
        print("Error closing socket : $err");
      } finally {
        _socket = null;
        disconnecting = false;
      }
    }
    print("Disconnected");
  }

  ///
  /// Add the provided audio data to the online encoder,
  /// connecting to the remote socket first if the connection has not yet been established.
  ///
  Future decode(Uint8List data) async {
    throw UnimplementedError(
        "This has been temporarily removed as all communication should occur via AudioPipeline");
  }

  void dispose() async {
    try {
      await disconnect();
    } catch (err) {
    } finally {
      _decodedController.close();
      _statusController.close();
    }
  }
}
