// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.tenta.xwalk.refactor;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * It's for http auth handling.
 * @hide
 */
@JNINamespace("xwalk")
public class XWalkHttpAuthHandler implements XWalkHttpAuth {

    private long mNativeXWalkHttpAuthHandler;
    private final boolean mFirstAttempt;

    @Override
    public void proceed(String username, String password) {
        if (mNativeXWalkHttpAuthHandler != 0) {
            nativeProceed(mNativeXWalkHttpAuthHandler, username, password);
            mNativeXWalkHttpAuthHandler = 0;
        }
    }

    @Override
    public void cancel() {
        if (mNativeXWalkHttpAuthHandler != 0) {
            nativeCancel(mNativeXWalkHttpAuthHandler);
            mNativeXWalkHttpAuthHandler = 0;
        }
    }

    @Override
    public boolean isFirstAttempt() {
         return mFirstAttempt;
    }

    @CalledByNative
    public static XWalkHttpAuthHandler create(long nativeXWalkAuthHandler, boolean firstAttempt) {
        return new XWalkHttpAuthHandler(nativeXWalkAuthHandler, firstAttempt);
    }

    public XWalkHttpAuthHandler(long nativeXWalkHttpAuthHandler, boolean firstAttempt) {
        mNativeXWalkHttpAuthHandler = nativeXWalkHttpAuthHandler;
        mFirstAttempt = firstAttempt;
    }

    // Never use this constructor.
    // It is only used in XWalkHttpAuthHandlerBridge.
    XWalkHttpAuthHandler() {
        mNativeXWalkHttpAuthHandler = 0;
        mFirstAttempt = false;
    }

    @CalledByNative
    void handlerDestroyed() {
        mNativeXWalkHttpAuthHandler = 0;
    }

    private native void nativeProceed(long nativeXWalkHttpAuthHandler,
            String username, String password);
    private native void nativeCancel(long nativeXWalkHttpAuthHandler);
}

