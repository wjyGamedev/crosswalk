package com.tenta.xwalk.refactor;

import org.chromium.android_webview.AwContentsClient;
import org.chromium.android_webview.AwContentsClientCallbackHelper;

import android.os.Looper;

public class AwXWalkContentsClientCallbackHelper extends AwContentsClientCallbackHelper {

    public AwXWalkContentsClientCallbackHelper(Looper looper, AwContentsClient contentsClient) {
        super(looper, contentsClient);
        // TODO Auto-generated constructor stub
    }

}
