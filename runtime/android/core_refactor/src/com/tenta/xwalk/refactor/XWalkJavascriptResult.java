// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.tenta.xwalk.refactor;

/**
 * This interface is used when XWalkUIClientInternal offers a JavaScript
 * modal dialog (alert, beforeunload or confirm) to enable the client to
 * handle the dialog in their own way. XWalkUIClientInternal will offer an object
 * that implements this interface to the client and when the client has handled
 * the dialog, it must either callback with confirm() or cancel() to allow
 * processing to continue.
 */
//TODO(iotto) : @XWalkAPI(instance = XWalkJavascriptResultHandlerInternal.class)
public interface XWalkJavascriptResult {
    /**
     * Handle a confirm with a result from caller.
     * @param result the result string from caller.
     * @since 1.0
     */
//TODO(iotto) :     @XWalkAPI
    public void confirmWithResult(String result);

    /**
     * Handle a confirm without a result.
     * @since 1.0
     */
//TODO(iotto) :     @XWalkAPI
    public void confirm();

    /**
     * Handle the result if the caller cancelled the dialog.
     * @since 1.0
     */
//TODO(iotto) :     @XWalkAPI
    public void cancel();
}
