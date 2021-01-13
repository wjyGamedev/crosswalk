// Copyright (c) 2016 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.tenta.xwalk.refactor;

/**
 * This class allows developers to determine whether any XWalkViewInternal used in the application has stored
 * data entered into text fields and to clear any such stored data for all XWalkViewInternal in thes application.
 */
public class XWalkViewDatabase {
    private static final Object sync = new Object();
    
    /**
     * Gets whether there is any saved data for web forms.
     *
     * @return whether there is any saved data for web forms
     * @see #clearFormData
     * @since 8.0
     */
    public static boolean hasFormData() {
        synchronized (sync) {
            return XWalkFormDatabase.hasFormData();
        }
    }

    /**
     * Clears any saved data for web forms.
     *
     * @see #hasFormData
     * @since 8.0
     */
    public static void clearFormData() {
        synchronized (sync) {
            XWalkFormDatabase.clearFormData();
        }
    }
}
