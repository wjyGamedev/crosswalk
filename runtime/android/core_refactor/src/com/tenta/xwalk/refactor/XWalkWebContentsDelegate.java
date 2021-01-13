// Copyright (c) 2013 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.tenta.xwalk.refactor;

import android.view.KeyEvent;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.components.embedder_support.delegate.WebContentsDelegateAndroid;


@JNINamespace("xwalk")
abstract class XWalkWebContentsDelegate extends WebContentsDelegateAndroid {
    @CalledByNative
    @Override
    public boolean shouldCreateWebContents(String targetUrl) {
        return true;
    }

    @CalledByNative
    public abstract boolean addNewContents(boolean isDialog, boolean isUserGesture);

    @CalledByNative
    @Override
    public abstract void closeContents();

    @CalledByNative
    @Override
    public abstract void activateContents();

    @CalledByNative
    @Override
    public abstract void rendererUnresponsive();

    @CalledByNative
    @Override
    public abstract void rendererResponsive();

    @CalledByNative
    @Override
    public abstract void handleKeyboardEvent(KeyEvent event);

    @CalledByNative
    @Override
    public abstract boolean addMessageToConsole(int level, String message,
            int lineNumber,String sourceId);


    @CalledByNative
    @Override
    public abstract void showRepostFormWarningDialog();

    @CalledByNative
    public abstract boolean shouldOverrideRunFileChooser(
            int processId, int renderId, int mode,
            String acceptTypes, String title, String defaultFilename, boolean capture);

    @CalledByNative
    public void updatePreferredSize(int widthCss, int heightCss) {
    }

//    @CalledByNative
    public void toggleFullscreen(boolean enterFullscreen) {
    }

    @CalledByNative
    public boolean isFullscreen() {
        return false;
    }
    
    @Override
    @CalledByNative
    public abstract void navigationStateChanged(int flags);
    
    @CalledByNative
    public abstract void setOverlayMode(boolean useOverlayMode);
}
