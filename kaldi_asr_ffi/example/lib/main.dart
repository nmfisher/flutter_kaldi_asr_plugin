import 'dart:io';

import 'package:archive/archive_io.dart';
import 'package:flutter/material.dart';
import 'dart:async';

import 'package:flutter/services.dart';
import 'package:kaldi_asr_ffi/ffi_flutter_kaldi_asr.dart';
import 'package:kaldi_asr_platform_interface/kaldi_asr_platform_interface.dart';
import 'package:path/path.dart';
import 'package:path_provider/path_provider.dart';

void main() {
  runApp(MyApp());
}

class MyApp extends StatefulWidget {
  @override
  _MyAppState createState() => _MyAppState();
}

class _MyAppState extends State<MyApp> {

  String wavPath;
  bool initialized = false;
  String decoded = null;
	bool decoding = false;

  static void unzipAsset(String path, String dest) async {
    var bytes = await rootBundle.load(path);
    unzipBytes(bytes, dest);
  }

  static void unzipBytes(ByteData bytes, String dest) {
    final archive = ZipDecoder().decodeBytes(bytes.buffer.asUint8List());
    for (final file in archive) {

      final filename = file.name;
      if (file.isFile) {
        final data = file.content as List<int>;
        File(join(dest, filename))
          ..createSync(recursive: true)
          ..writeAsBytesSync(data);
      } else {
        Directory(join(dest, filename))
          ..create(recursive: true);
      }
    }
  }
  
  void initialize() async {
    var tempDir = (await getTemporaryDirectory()).path;
    var outDir = join(tempDir, "model");
    unzipAsset("assets/cvte.zip", outDir);
    
    
    KaldiAsrPlatform.instance = FFIKaldiAsrPlatform(outDir, tempDir);
    var fstFile = File(join(outDir, "HCLG.fst"));
    var fstData = await rootBundle.load("assets/HCLG.fst");
    fstFile.writeAsBytesSync(fstData.buffer.asUint8List());
    KaldiAsrPlatform.instance.initialize(fstFile.path);
    // the decoder will normally work with temp files on the filesystem, so we'll copy the asset to the temporary 
     var bytes = await rootBundle.load("assets/3.wav");
     wavPath = join(tempDir, "3.wav");
     File(wavPath).writeAsBytesSync(bytes.buffer.asUint8List());
     setState(() {
       initialized = true;
     });
     
  }

  @override
  void initState() {
    super.initState();
    initialize();
  }
    
  void decode() async {
	decoded = await KaldiAsrPlatform.instance.decode(wavPath);
 	setState(() {
        	decoding = false;
    });
  }
  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      home: Scaffold(
        appBar: AppBar(
          title: const Text('Plugin example app'),
        ),
        body: Center(
          child: initialized ? Column(children:[
            RaisedButton(child:Text('Decode wav'), onPressed: () {
                setState(() {
		    decoding = true;
                    decoded = null;
                });
                decode();  
            },),
            decoded == null ? Container() : Text(decoded)
        ]) : CircularProgressIndicator(),
        ),
      ),
    );
  }
}
