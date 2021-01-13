// Copyright (c) 2013-2014 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.tenta.xwalk.refactor;

import org.chromium.content_public.browser.NavigationEntry;

/**
 * This class represents a navigation item and is managed in XWalkNavigationHistoryInternal.
 */
//TODO(iotto) : @XWalkAPI(createInternally = true)
public class XWalkNavigationItem implements Cloneable {
    private NavigationEntry mEntry;

    // Never use this constructor.
    // It is only used in XWalkNavigationItemBridge.
    XWalkNavigationItem() {
        mEntry = null;
    }

    XWalkNavigationItem(NavigationEntry entry) {
        mEntry = entry;
    }

    XWalkNavigationItem(XWalkNavigationItem item) {
        mEntry = item.mEntry;
    }

    /**
     * Get the url of current navigation item.
     * @return the string of the url.
     * @since 1.0
     */
    public String getUrl() {
        return mEntry.getUrl();
    }

    /**
     * Get the original url of current navigation item.
     * @return the string of the original url.
     * @since 1.0
     */
    public String getOriginalUrl() {
        return mEntry.getOriginalUrl();
    }

    /**
     * Get the title of current navigation item.
     * @return the string of the title.
     * @since 1.0
     */
    public String getTitle() {
        return mEntry.getTitle();
    }

    @Override
    protected synchronized XWalkNavigationItem clone() {
        return new XWalkNavigationItem(this);
    }
}
