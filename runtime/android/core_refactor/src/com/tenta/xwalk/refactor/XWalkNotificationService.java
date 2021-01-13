// Copyright (c) 2014 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.tenta.xwalk.refactor;

import android.content.Intent;
import android.graphics.Bitmap;

interface XWalkNotificationService {
    public void setBridge(XWalkContentsClientBridge bridge);
    public void showNotification(
            String title, String message, String replaceId, Bitmap icon, int notificationId);
    public void cancelNotification(int notificationId);
    public void shutdown();
    public boolean maybeHandleIntent(Intent intent);
}
