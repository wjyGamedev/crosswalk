// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.pakdata.xwalk.refactor;

import java.util.HashMap;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

import java.util.HashMap;
import java.util.Map;

import android.util.Log;

/**
 * Delegate for handling callbacks. All methods are called on the IO thread.
 */
@JNINamespace("xwalk")
public abstract class XWalkContentsIoThreadClient {
    @CalledByNative
    public abstract int getCacheMode();

    @CalledByNative
    public abstract boolean shouldBlockContentUrls();

    @CalledByNative
    public abstract boolean shouldBlockFileUrls();

    @CalledByNative
    public abstract boolean shouldBlockNetworkLoads();

    @CalledByNative
    public abstract boolean shouldAcceptThirdPartyCookies();
    
    // ch77
    @CalledByNative
    public abstract XWalkContentsBackgroundThreadClient getBackgroundThreadClient();
}
