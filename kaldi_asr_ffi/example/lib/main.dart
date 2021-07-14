import 'package:flutter/material.dart';
import 'package:kaldi_asr_ffi/online_kaldi_decoder.dart';
import 'package:path_provider/path_provider.dart';

void main() {
  runApp(MyApp());
}

class MyApp extends StatefulWidget {
  @override
  _MyAppState createState() => _MyAppState();
}

class _MyAppState extends State<MyApp> {
  late OnlineKaldiDecoder _asr;

  @override
  void initState() {
    super.initState();
    getTemporaryDirectory().then((dir) async {
      _asr = OnlineKaldiDecoder(dir.path, 16000);
      await _asr.initialize();
      print("initialized!");
    });
  }

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      home: Scaffold(
        appBar: AppBar(
          title: const Text('Plugin example app'),
        ),
        body: Center(child: RaisedButton(
          onPressed: () {
            _asr.loadFSTFromAsset("demo.fst");
          },
        )),
      ),
    );
  }
}
