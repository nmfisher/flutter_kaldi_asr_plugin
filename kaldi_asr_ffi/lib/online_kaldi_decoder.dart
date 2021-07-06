import 'package:flutter/services.dart';
import 'dart:async';
import 'dart:convert';
import 'dart:io';
import 'dart:typed_data';
import 'package:kaldi_asr_platform_interface/kaldi_asr_platform_interface.dart';
import 'package:path/path.dart';

class OnlineKaldiDecoder extends KaldiAsrPlatform {
  bool debug;

  static const MethodChannel _channel =
      const MethodChannel('com.avinium.kaldi_asr_ffi');

  String _logDir;
  double _sampFreq;

  Future<Socket> _socket;
  int _portNum;
  StreamSubscription _listener;

  Stream<String> get decoded => _decodedController.stream;
  StreamController<String> _decodedController =
      StreamController<String>.broadcast();

  Stream<ConnectionStatus> get status => _statusController.stream;
  StreamController<ConnectionStatus> _statusController =
      StreamController<ConnectionStatus>();

  /// Constructs an uninitialized plugin instance.
  /// Accepts a single (optional) argument representing the path to a zip file (e.g. "assets/cvte.zip") containing:
  /// - the acoustic model (final.mdl)
  /// - the word symbol table (words.txt)
  /// - all FSTs (named according to whatever convention you intend on using: e.g. 1.fst, HCLG.fst)
  /// - all configuration/option files (global_cmvn.stas, cmvn_opts, fbank.conf, etc)
  /// - a config file containing all parameters that would normally be passed on the command line to the Kaldi decoder (file must be named config)
  ///     - parameters must be separated by a newline,  e.g. --cmvn-config=%MODEL_DIR%/cmvn_opts\n--feature-type=fbank\n
  /// The above files must be located in the top-level directory of the zip file (i.e no subfolders).
  /// Invoking code must call the initialize() function before using the class to perform any work.
  OnlineKaldiDecoder(this._logDir, this._sampFreq, {this.debug = true});

  void _debug(String message) {
    if (debug) print(message);
  }

  Future loadFST(String fstFilename) async {   
    _listener?.cancel();

    var retCode = await _channel.invokeMethod('loadFST', {"fst": fstFilename}); 
    print("Got retCode $retCode");
  }

  ///
  /// Initializes this plugin instance with the provided FST path.
  /// It is safe to call this method multiple times, with different FST paths.
  ///
  Future initialize() async {

    var logfile = join(_logDir, "log");
    _debug("Writing to $logfile");

    _debug("Invoking initialization function");

    _portNum = await _channel.invokeMethod('initialize',
        { "log": logfile, "sampleFrequency": _sampFreq});
    if (_portNum < 0)
      throw Exception(
          "Unknown error initializing Kaldi plugin. Check log for further details");
    _debug("Decoder successfully configured and listening on port $_portNum");
  }

  ///
  /// Connect to the remote online decoder socket. This does not need to be invoked manually,
  /// as [decode] will invoke method to establish a connection before any data is sent.
  /// However, invoking this method manually might be useful if you want to minimize the overhead in sending data to the decoder.
  ///
  Future connect() async {
    if ((await _socket) != null) return;

    try {
      _listener?.cancel();
      print("Connecting to socket on port $_portNum");
      _socket = Socket.connect(InternetAddress.loopbackIPv4, _portNum,
          timeout: Duration(seconds: 30));
      _listener = (await _socket).listen((data) async {
        var decoded = utf8.decode(data);

        _decodedController.add(decoded);
        if (decoded.endsWith('\n')) {
          print("Final decode result received : [ $decoded ]");
          _listener?.cancel();
          await disconnect();
        }
      });
      print("Connected.");
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
    print("Disconnecting socket");
    if (_socket == null) {
      print("Null socket, returning");
      return;
    }
    disconnecting = true;
    try {
      await (await _socket).flush();
      print("Flushed");
    } catch (err) {
      print("Error flushing socket : $err");
    } finally {
      try {
        await (await _socket).close();
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
    if (!disconnecting) (await _socket)?.add(data);
  }


}
