package com.tenta.xwalk.refactor;

import org.chromium.android_webview.AwBrowserContext;
import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwContentsClient;
import org.chromium.android_webview.AwSettings;

import android.content.Context;
import android.view.ViewGroup;

public class AwXWalkContents extends AwContents {

    public AwXWalkContents(AwBrowserContext browserContext, ViewGroup containerView,
            Context context, InternalAccessDelegate internalAccessAdapter,
            NativeDrawFunctorFactory nativeDrawFunctorFactory, AwContentsClient contentsClient,
            AwSettings awSettings) {
        super(browserContext, containerView, context, internalAccessAdapter, nativeDrawFunctorFactory,
                contentsClient, awSettings);
        // TODO Auto-generated constructor stub
    }

}
