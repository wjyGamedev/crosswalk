package com.pakdata.xwalk.refactor;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

@JNINamespace("xwalk")
public abstract class XWalkContentsBackgroundThreadClient {

    public abstract XWalkWebResourceResponse shouldInterceptRequest(
            XWalkContentsClient.XWalkWebResourceRequest request);

    // Protected methods ---------------------------------------------------------------------------

    @CalledByNative
    private XWalkWebResourceResponse shouldInterceptRequestFromNative(String url,
            boolean isMainFrame,
            boolean hasUserGesture, String method, String[] requestHeaderNames,
            String[] requestHeaderValues) {
        return shouldInterceptRequest(new XWalkContentsClient.XWalkWebResourceRequest(
                url, isMainFrame, hasUserGesture, method, requestHeaderNames, requestHeaderValues));
    }
}
