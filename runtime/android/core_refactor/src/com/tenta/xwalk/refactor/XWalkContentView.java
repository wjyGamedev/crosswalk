// Copyright 2012 The Chromium Authors. All rights reserved.
// Copyright (c) 2015 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.tenta.xwalk.refactor;

import android.content.Context;
import android.graphics.Rect;
import android.os.Build;
import android.view.MotionEvent;
import android.view.ViewStructure;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsAccessibility;


public class XWalkContentView extends ContentView {
    private XWalkView mXWalkView;

    public static XWalkContentView createContentView(Context context, WebContents webContents, XWalkView xwView) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            return new XWalkContentViewApi23(context, webContents, xwView);
        }
        return new XWalkContentView(context, webContents, xwView);
    }

    private XWalkContentView(Context context, WebContents webContents, XWalkView xwView) {
        super(context, webContents);
        mXWalkView = xwView;
    }

    @Override
    public InputConnection onCreateInputConnection(EditorInfo outAttrs) {
        return mXWalkView.onCreateInputConnection(outAttrs);
    }

    public InputConnection onCreateInputConnectionSuper(EditorInfo outAttrs) {
        return super.onCreateInputConnection(outAttrs);
    }

    @Override
    public boolean performLongClick() {
        return mXWalkView.performLongClickDelegate();
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        // Give XWalkView a chance to handle touch event
        if (mXWalkView.onTouchEventDelegate(event)) {
            return true;
        }
        return super.onTouchEvent(event);
    }

    @Override
    public void onScrollChanged(int l, int t, int oldl, int oldt) {
        mXWalkView.onScrollChangedDelegate(l, t, oldl, oldt);

        // To keep the same behaviour with WebView onOverScrolled API,
        // call onOverScrolledDelegate here.
        mXWalkView.onOverScrolledDelegate(l, t, false, false);
    }

    /**
     * Since compute* APIs in ContentView are all protected, use delegate methods
     * to get the result.
     */
    public int computeHorizontalScrollRangeDelegate() {
        return computeHorizontalScrollRange();
    }

    public int computeHorizontalScrollOffsetDelegate() {
        return computeHorizontalScrollOffset();
    }

    public int computeVerticalScrollRangeDelegate() {
        return computeVerticalScrollRange();
    }

    public int computeVerticalScrollOffsetDelegate() {
        return computeVerticalScrollOffset();
    }

    public int computeVerticalScrollExtentDelegate() {
        return computeVerticalScrollExtent();
    }

    @Override
    protected void onFocusChanged(boolean gainFocus, int direction, Rect previouslyFocusedRect) {
        mXWalkView.onFocusChangedDelegate(gainFocus, direction, previouslyFocusedRect);
        super.onFocusChanged(gainFocus, direction, previouslyFocusedRect);
    }

    // Imitate ContentView.ContentViewApi23
    private static class XWalkContentViewApi23 extends XWalkContentView {
        public XWalkContentViewApi23(Context context, WebContents webContents, XWalkView xwView) {
            super(context, webContents, xwView);
        }

        @Override
        public void onProvideVirtualStructure(final ViewStructure structure) {
            WebContentsAccessibility wcax = getWebContentsAccessibility();
            if (wcax != null) wcax.onProvideVirtualStructure(structure, false);
        }
    }
}
