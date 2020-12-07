import 'dart:io';
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
  
  FFIKaldiAsrPlatform _asr;

  @override
  void initState() {
    super.initState();
    _asr = FFIKaldiAsrPlatform("outDir", "tempDir");
  }
    
  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      home: Scaffold(
        appBar: AppBar(
          title: const Text('Plugin example app'),
        ),
        body: Center(
          child: Text("loaded")
        ),
      ),
    );
  }
}
