import 'dart:async';
import 'dart:convert';
import 'dart:ffi';
import 'dart:io';
import 'dart:typed_data';
import 'package:kaldi_asr_platform_interface/kaldi_asr_platform_interface.dart';
import 'package:path/path.dart';
import 'package:random_string/random_string.dart';
import 'package:ffi/ffi.dart';

DynamicLibrary dl;

typedef InitializeFunction = Int32 Function(Int32, Pointer<Pointer<Utf8>>);
typedef InitializeFunctionDart = int Function(int, Pointer<Pointer<Utf8>>);

class OnlineKaldiDecoder extends KaldiAsrPlatform {
  bool debug;

  InitializeFunctionDart _initFn;

  String _initString = '''--global-cmvn-stats=%MODEL_DIR%/global_cmvn
  --cmvn-config=%MODEL_DIR%/cmvn_opts 
  --feature-type=fbank 
  --fbank-config=%MODEL_DIR%/fbank.conf 
  --acoustic-scale=1.0 
  --frame-subsampling-factor=3 
  --frames-per-chunk=50
  --minimize=false
  --word-symbol-table=%MODEL_DIR%/words.txt
  --samp-freq=%SAMP_FREQ%
  --endpoint.silence-phones=1
  %MODEL_DIR%/final.mdl 
  %FST_PATH%''';

  List<String> _args;

  String _modelDir;
  double _sampFreq;

  Socket _socket;
  int _portNum;
  StreamSubscription _listener;

  Stream<String> get decoded => _decodedController.stream;
  StreamController<String> _decodedController =
      StreamController<String>.broadcast();

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
  OnlineKaldiDecoder(this._modelDir, this._sampFreq, {this.debug = true}) {
    assert(this._modelDir != null);
    dl = Platform.isAndroid
        ? DynamicLibrary.open("libasrbridge.so")
        : DynamicLibrary.process();
    _initFn = dl.lookupFunction<InitializeFunction, InitializeFunctionDart>(
        "initializeFFI");
  }

  void _debug(String message) {
    if (debug) print(message);
  }

  ///
  /// Initializes this plugin instance with the provided FST path.
  /// It is safe to call this method multiple times, with different FST paths.
  ///
  Future initialize(String fstPath) async {
    _listener?.cancel();

    if (!File(fstPath).existsSync())
      throw Exception("Specified FST path does not exist");

    var args = _initString
        .replaceAll("%FST_PATH%", fstPath)
        .replaceAll("%MODEL_DIR%", _modelDir)
        .replaceAll("%SAMP_FREQ%", _sampFreq.toString())
        .split("\n");
    var argArray = allocate<Pointer<Utf8>>(count: args.length + 1);

    var logfile = join(_modelDir, "log");
    _debug("Writing to $logfile");
    argArray.elementAt(0).value = Utf8.toUtf8(
        logfile); // normally the executable at argv[0], but we're invoking via a library. temporary only -  use this for a log file to redirect stderr
    int numArgs = args.length + 1;

    for (int i = 1; i < numArgs; i++)
      argArray.elementAt(i).value = Utf8.toUtf8(args[i - 1].trim());

    _debug(
        "Invoking initialization function with $numArgs args : [ $argArray ]");
    _portNum = _initFn(numArgs, argArray);
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
    _listener?.cancel();
    print("Connecting to socket on port $_portNum");
    _socket = await Socket.connect(InternetAddress.loopbackIPv4, _portNum,
        timeout: Duration(seconds: 30));
    _listener = _socket.listen((data) {
      _decodedController.add(utf8.decode(data));
    });
    print("Connected.");
  }

  ///
  /// Disconnect from the remote online decoder socket.
  ///
  Future disconnect() async {
    if (_socket != null) {
      try {
        await _socket.flush();
        await _socket.close();
      } finally {
        _socket = null;
      }
    }
    print("Disconnected");
  }

  ///
  /// Add the provided audio data to the online encoder,
  /// connecting to the remote socket first if the connection has not yet been established.
  ///
  Future decode(Uint8List data) async {
    if (_socket == null) await connect();
    _socket.add(data);
  }
}
