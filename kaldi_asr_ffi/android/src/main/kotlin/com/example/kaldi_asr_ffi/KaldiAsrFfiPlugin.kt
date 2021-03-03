package com.example.kaldi_asr_ffi

import androidx.annotation.NonNull;

import io.flutter.embedding.engine.plugins.FlutterPlugin
import io.flutter.plugin.common.MethodCall
import io.flutter.plugin.common.MethodChannel
import io.flutter.plugin.common.MethodChannel.MethodCallHandler
import io.flutter.plugin.common.MethodChannel.Result
import io.flutter.plugin.common.PluginRegistry.Registrar

import android.content.res.AssetManager
import android.content.Context;

import io.flutter.embedding.engine.plugins.activity.ActivityAware

/** FlutterKaldiAsrPlugin */
public class KaldiAsrFfiPlugin: FlutterPlugin, MethodCallHandler {

  private lateinit var channel : MethodChannel
  private lateinit var context: Context
  
  companion object {
    init {
      System.loadLibrary("asrbridge");
    }
  }

  override fun onAttachedToEngine(@NonNull flutterPluginBinding: FlutterPlugin.FlutterPluginBinding) {
    println("Attached to engine")

    channel = MethodChannel(flutterPluginBinding.binaryMessenger, "com.avinium.kaldi_asr_ffi")
    channel.setMethodCallHandler(this)
    context = flutterPluginBinding.applicationContext
  }

  override fun onMethodCall(@NonNull call: MethodCall, @NonNull result: Result) {
    var am = context.getAssets()
    if (call.method == "initialize") {
      var portNum = initialize(am, call.argument<String>("log")!!, call.argument<Int>("sampleFrequency")!!)
      result.success(portNum)
    } else if(call.method == "loadFST") {
      var fst = call.argument<String>("fst")!!
      var retCode = loadFST(am, fst)
      result.success(retCode);
    } else {
      result.notImplemented()
    }
  }

  override fun onDetachedFromEngine(@NonNull binding: FlutterPlugin.FlutterPluginBinding) {
    channel.setMethodCallHandler(null)
  }

  external fun initialize(mgr: AssetManager, log: String, sampleFrequency: Int) : Int;
  external fun loadFST(mgr: AssetManager, fst: String) : Int;
}
