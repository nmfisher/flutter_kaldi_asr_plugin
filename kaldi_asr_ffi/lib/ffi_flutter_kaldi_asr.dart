import 'dart:async';
import 'dart:ffi';
import 'dart:io';
import 'package:kaldi_asr_platform_interface/kaldi_asr_platform_interface.dart';
import 'package:path/path.dart';
import 'package:random_string/random_string.dart';
import 'package:ffi/ffi.dart';

DynamicLibrary fst = Platform.isAndroid ? DynamicLibrary.open("libfst.so") : DynamicLibrary.process();
DynamicLibrary dl = Platform.isAndroid ? DynamicLibrary.open("libasrbridge.so") : DynamicLibrary.process();

typedef InitializeFunction = Int32 Function(
    Int32, Pointer<Pointer<Utf8>>);
typedef InitializeFunctionDart = int Function(
    int, Pointer<Pointer<Utf8>>);

typedef DecodeFunction = Void Function(
    Pointer<Utf8>, Pointer<Utf8>);
typedef DecodeFunctionDart = void Function(
    Pointer<Utf8>, Pointer<Utf8>);


class FFIKaldiAsrPlatform extends KaldiAsrPlatform {

  InitializeFunctionDart _initFn;
  DecodeFunctionDart _decodeFn;

  String _initString = '''--global-cmvn-stats=%MODEL_DIR%/global_cmvn
  --cmvn-config=%MODEL_DIR%/cmvn_opts 
  --feature-type=fbank 
  --fbank-config=%MODEL_DIR%/fbank.conf 
  --acoustic-scale=1.0 
  --do-endpointing=false 
  --frame-subsampling-factor=3 
  --frames-per-chunk=50
  --minimize=false
  --word-symbol-table=%MODEL_DIR%/words.txt
  %MODEL_DIR%/final.mdl 
  %FST_PATH%''';
  
  List<String> _args;
  String _error;

  String modelDir; 
  String tempDir;

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
  FFIKaldiAsrPlatform(this.modelDir, this.tempDir) {
    assert(this.modelDir != null);
    assert(this.tempDir != null);
    _initFn = dl.lookupFunction<InitializeFunction, InitializeFunctionDart>(
          "initializeFFI");
    _decodeFn = dl.lookupFunction<DecodeFunction, DecodeFunctionDart>(
          "decode");
  }


  /// Initializes this plugin instance with the provided FST path.
  /// It is safe to call this method multiple times, with different FST paths.
  Future initialize(String fstPath) async {
    if(!File(fstPath).existsSync())
      throw Exception("Specified FST path does not exist");

    var args = _initString.replaceAll("%FST_PATH%", fstPath).replaceAll("%MODEL_DIR%", modelDir).split("\n");
    var argArray = allocate<Pointer<Utf8>>(count:args.length+1);
    
    var logfile = join(modelDir, "log");
    print("Writing to $logfile");
    argArray.elementAt(0).value = Utf8.toUtf8(logfile); // normally the executable at argv[0], but we're invoking via a library. temporary only -  use this for a log file to redirect stderr
    int numArgs = args.length + 1;

    for (int i = 1; i < numArgs; i++)
      argArray.elementAt(i).value = Utf8.toUtf8(args[i-1].trim());
    if(_initFn(numArgs, argArray) != 0)
        throw Exception("Unknown error initializing Kaldi plugin. Check log for further details");
  }
  
  /// 
  /// Decode the audio at the provided path.
  /// If [wordIds] is false, the word IDs will be returned (if true, the word-strings will be returned)
  /// 
  Future<List<String>> decode(String wavPath, [bool wordIds=false]) async {
    if(!await File(wavPath).exists())
      throw Exception("Specified file does not exist");

    var transcriptPath = join(tempDir, randomAlphaNumeric(10));
    
    var arkPath =  join(tempDir, randomAlphaNumeric(10));
    var arkFile = File(arkPath);
    await arkFile.writeAsString("000 $wavPath\n");
    print("Decoding file at ${await arkFile.readAsString()}");
    _decodeFn(Utf8.toUtf8("scp:$arkPath"), Utf8.toUtf8(transcriptPath));
    arkFile.delete();
    var transcriptFile = File(transcriptPath);
    if(! await transcriptFile.exists())
      // means decoding failed
      return null;
    // the decoder will write the transcript by writing one line containing a single word and word ID (separated by a tab).
    var content = await transcriptFile.readAsString();
    var lines = content.split("\n");
    List<String> transcript = [];
    // skip any empty lines
    for(int i = 0; i < lines.length - 1; i++) { 
      var split = lines[i].split("\t");
      if(split.length > 1)
        transcript.add(split[wordIds ? 1 :0]); 
    }
    transcriptFile.delete();
    return transcript;
  }

}
