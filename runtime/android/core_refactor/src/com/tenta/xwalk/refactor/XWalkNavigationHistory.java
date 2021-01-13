// Copyright (c) 2013-2014 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.tenta.xwalk.refactor;

import java.io.Serializable;

import org.chromium.content_public.browser.NavigationHistory;

/**
 * This class represents a navigation history for a XWalkView instance.
 * It's not thread-safe and should be only called on UI thread.
 */
//TODO(iotto) : @XWalkAPI(createInternally = true)
public class XWalkNavigationHistory implements Cloneable, Serializable {
    private NavigationHistory mHistory;
    private XWalkView mXWalkView;

    // Never use this constructor.
    // It is only used in XWalkNavigationHistoryBridge.
    XWalkNavigationHistory() {
        mXWalkView = null;
        mHistory = null;
    }

    XWalkNavigationHistory(XWalkView view, NavigationHistory history) {
        mXWalkView = view;
        mHistory = history;
    }

    XWalkNavigationHistory(XWalkNavigationHistory history) {
        mXWalkView = history.mXWalkView;
        mHistory = history.mHistory;
    }

    /**
     * Total size of navigation history for the XWalkView.
     * @return the size of total navigation items.
     * @since 1.0
     */
//TODO(iotto) :     @XWalkAPI
    public int size() {
        return mHistory.getEntryCount();
    }

    /**
     * Test whether there is an item at a specific index.
     * @param index the given index.
     * @return true if there is an item at the specific index.
     * @since 1.0
     */
//TODO(iotto) :     @XWalkAPI
    public boolean hasItemAt(int index) {
        return index >=0 && index <= size() - 1;
    }

    /**
     * Get a specific item given by index.
     * @param index the given index.
     * @return the navigation item for the given index.
     * @since 1.0
     */
//TODO(iotto) :     @XWalkAPI
    public XWalkNavigationItem getItemAt(int index) {
        if (index < 0 || index >= size()) return null;
        return new XWalkNavigationItem(mHistory.getEntryAtIndex(index));
    }

    /**
     * Get the current item which XWalkView displays.
     * @return the current navigation item.
     * @since 1.0
     */
//TODO(iotto) :     @XWalkAPI
    public XWalkNavigationItem getCurrentItem() {
        return getItemAt(getCurrentIndex());
    }

    /**
     * Test whether XWalkView can go back.
     * @return true if it can go back.
     * @since 1.0
     */
//TODO(iotto) :     @XWalkAPI
    public boolean canGoBack() {
        return mXWalkView.canGoBack();
    }

    /**
     * Test whether XWalkView can go forward.
     * @return true if it can go forward.
     * @since 1.0
     */
//TODO(iotto) :     @XWalkAPI
    public boolean canGoForward() {
        return mXWalkView.canGoForward();
    }

//TODO(iotto) :     @XWalkAPI
    public boolean removeHistoryEntryAt(int index) {
    	return mXWalkView.removeHistoryEntryAt(index);
    }
    
    /**
     * The direction for web page navigation.
     * @since 1.0
     */
//TODO(iotto) :     @XWalkAPI
    public enum DirectionInternal {
        /** The backward direction for web page navigation. */
        BACKWARD,
        /** The forward direction for web page navigation. */
        FORWARD
    }

    /**
     * Navigates to the specified step from the current navigation item.
     * Do nothing if the offset is out of bound.
     * @param direction the direction of navigation.
     * @param steps go back or foward with a given steps.
     * @since 1.0
     */
//TODO(iotto) :     @XWalkAPI
    public void navigate(DirectionInternal direction, int steps) {
        switch(direction) {
            case FORWARD:
                mXWalkView.navigateTo(steps);
                break;
            case BACKWARD:
                mXWalkView.navigateTo(-steps);
                break;
            default:
                break;
        }
    }

    /**
     * Get the index for current navigation item.
     * @return current index in the navigation history.
     * @since 1.0
     */
    public int getCurrentIndex() {
        return mHistory.getCurrentEntryIndex();
    }

    /**
     * Clear all history owned by this XWalkView.
     * @since 1.0
     */
    public void clear() {
        mXWalkView.clearHistory();
    }

    @Override
    protected synchronized XWalkNavigationHistory clone() {
        return new XWalkNavigationHistory(this);
    }
}
