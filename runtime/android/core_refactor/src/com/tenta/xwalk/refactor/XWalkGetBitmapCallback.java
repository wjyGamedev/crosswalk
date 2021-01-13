// Copyright (c) 2015 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.tenta.xwalk.refactor;

import android.graphics.Bitmap;

/**
 * A callback for content readback into a bitmap.
 */
public abstract class XWalkGetBitmapCallback {
    /**
     * Constructor for capture bitmap callback.
     * @since 6.0
     */
    public XWalkGetBitmapCallback() {}

    /**
     * Called when the content readback finishes.
     * @param bitmap The bitmap of the content. Null will be passed for
     * readback failure.
     * @param response 0 for success, others failure.
     * @since 6.0
     */
    public abstract void onFinishGetBitmap(Bitmap bitmap, int response);
}
