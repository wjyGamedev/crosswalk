// Copyright (c) 2015 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.tenta.xwalk.refactor;

import org.xwalk.core.internal.extensions.XWalkNativeExtensionLoaderAndroid;

public class XWalkNativeExtensionLoader extends XWalkNativeExtensionLoaderAndroid {
    /**
     * register path of native extensions.
     * @param path the path of the extension.
     */
    @Override
    public void registerNativeExtensionsInPath(String path) {
        super.registerNativeExtensionsInPath(path);
    }
}
