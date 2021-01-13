package com.tenta.xwalk.refactor;

import org.chromium.android_webview.AwActionModeCallback;
import org.chromium.android_webview.AwContents;
import org.chromium.content_public.browser.WebContents;

import android.content.Context;

public class AwXWalkActionModeCallback extends AwActionModeCallback {

    public AwXWalkActionModeCallback(Context context, AwContents awContents,
            WebContents webContents) {
        super(context, awContents, webContents);
        // TODO Auto-generated constructor stub
    }

}
