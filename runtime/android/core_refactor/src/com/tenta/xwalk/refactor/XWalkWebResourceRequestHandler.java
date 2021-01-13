// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.tenta.xwalk.refactor;

import android.net.Uri;

import java.util.Map;

public class XWalkWebResourceRequestHandler implements XWalkWebResourceRequest {
    private final XWalkContentsClient.XWalkWebResourceRequest mRequest;

    // Never use this constructor.
    // It is only used in WebResourceRequestHandlerBridge.
    XWalkWebResourceRequestHandler() {
        mRequest = null;
    }

    XWalkWebResourceRequestHandler(
            XWalkContentsClient.XWalkWebResourceRequest request) {
        mRequest = request;
    }

    @Override
    public Uri getUrl() {
        return Uri.parse(mRequest.url);
    }

    @Override
    public boolean isForMainFrame() {
        return mRequest.isMainFrame;
    }

    @Override
    public boolean hasGesture() {
        return mRequest.hasUserGesture;
    }

    @Override
    public String getMethod() {
        return mRequest.method;
    }

    @Override
    public Map<String, String> getRequestHeaders() {
        return mRequest.requestHeaders;
    }
}
