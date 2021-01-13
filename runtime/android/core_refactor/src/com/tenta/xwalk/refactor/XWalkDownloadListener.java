// Copyright (c) 2015 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.tenta.xwalk.refactor;

import android.content.Context;


public abstract class XWalkDownloadListener {

    /**
     * Constructor for download listener.
     * @param context  a Context object.
     * @since 5.0
     */
    
    public XWalkDownloadListener(Context context) {}

    /**
     * Notify the host application that a file should be downloaded.
     * @param url The full url to the content that should be downloaded
     * @param userAgent the user agent to be used for the download.
     * @param contentDisposition Content-disposition http header, if present.
     * @param mimetype The mimetype of the content reported by the server
     * @param contentLength The file size reported by the server
     * @since 5.0
     */
    
    public abstract void onDownloadStart(String url, String userAgent,
            String contentDisposition, String mimetype, long contentLength);
}
