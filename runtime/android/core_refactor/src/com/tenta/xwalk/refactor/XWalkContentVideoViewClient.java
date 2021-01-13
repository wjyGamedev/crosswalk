// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.tenta.xwalk.refactor;

import android.app.Activity;
import android.content.Context;
import android.view.View;
import android.view.WindowManager;

import org.chromium.base.CommandLine;
import org.chromium.content.browser.ContentVideoView;
//import org.chromium.content.browser.ContentVideoViewEmbedder;
import org.chromium.content_public.browser.ContentVideoViewEmbedder;
import org.chromium.content.common.ContentSwitches;

import com.tenta.xwalk.refactor.CustomViewCallbackHandler;

class XWalkContentVideoViewClient implements ContentVideoViewEmbedder {
    private XWalkContentsClient mContentsClient;
    private XWalkView mView;

    public XWalkContentVideoViewClient(XWalkContentsClient client, XWalkView view) {
        mContentsClient = client;
        mView = view;
    }

    @Override
    public void enterFullscreenVideo(View view, boolean isVideoLoaded) {
        mView.setOverlayVideoMode(true);
        mContentsClient.onShowCustomView(view, new CustomViewCallbackHandler());
        //TODO(iotto) add progress bar if video not loaded
    }

    @Override
    public void fullscreenVideoLoaded() {
    }

    @Override
    public void exitFullscreenVideo() {
        mView.setOverlayVideoMode(false);
        mContentsClient.onHideCustomView();
    }

    @Override
    public void setSystemUiVisibility(boolean enterFullscreen) {
    }
}
