// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.tenta.xwalk.refactor;

import org.chromium.base.ThreadUtils;

//TODO(iotto) : @XWalkAPI(impl = XWalkJavascriptResult.class, createInternally = true)
public class XWalkJavascriptResultHandler implements XWalkJavascriptResult {
    private XWalkContentsClientBridge mBridge;
    private final int mId;

    XWalkJavascriptResultHandler(XWalkContentsClientBridge bridge, int id) {
        mBridge = bridge;
        mId = id;
    }

    // Never use this constructor.
    // It is only used in XWalkJavascriptResultHandlerBridge.
    XWalkJavascriptResultHandler() {
        mBridge = null;
        mId = -1;
    }

    @Override
//TODO(iotto) :     @XWalkAPI
    public void confirm() {
        confirmWithResult(null);
    }

    @Override
//TODO(iotto) :     @XWalkAPI
    public void confirmWithResult(final String promptResult) {
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                if (mBridge != null) {
                    mBridge.confirmJsResult(mId, promptResult);
                }
                mBridge = null;
            }
        });
    }

    @Override
//TODO(iotto) :     @XWalkAPI
    public void cancel() {
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                if (mBridge != null) {
                    mBridge.cancelJsResult(mId);
                }
                mBridge = null;
            }
        });
    }
}
