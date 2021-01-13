// Copyright (c) 2016 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.tenta.xwalk.refactor;

/**
 * A callback interface used by the host application to notify
 * the current page that its custom view has been dismissed.
 */
public interface CustomViewCallback {
    /**
     * Invoked when the host application dismisses the
     * custom view.
     * @since 7.0
     */
    public void onCustomViewHidden();
}
