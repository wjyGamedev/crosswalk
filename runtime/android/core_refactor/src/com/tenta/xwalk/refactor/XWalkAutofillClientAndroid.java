// Copyright 2014 The Chromium Authors. All rights reserved.
// Copyright (c) 2015 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.tenta.xwalk.refactor;

import android.view.ViewGroup;
import android.view.View;
import android.content.Context;
import android.graphics.Color;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.components.autofill.AutofillDelegate;
import org.chromium.components.autofill.AutofillPopup;
import org.chromium.components.autofill.AutofillSuggestion;
import org.chromium.ui.DropdownItem;
import org.chromium.ui.base.WindowAndroid;

/**
 * Java counterpart to the XWalkAutofillClient. This class is owned by XWalkContent and has a weak
 * reference from native side.
 */
@JNINamespace("xwalk")
public class XWalkAutofillClientAndroid {

    private final long mNativeXWalkAutofillClientAndroid;
    private AutofillPopup mAutofillPopup;
    private Context mContext;

    @CalledByNative
    public static XWalkAutofillClientAndroid create(long nativeClient) {
        return new XWalkAutofillClientAndroid(nativeClient);
    }

    private XWalkAutofillClientAndroid(long nativeXWalkAutofillClient) {
        mNativeXWalkAutofillClientAndroid = nativeXWalkAutofillClient;
    }

    public void init(Context context) {
        mContext = context;
    }

    @CalledByNative
    private void showAutofillPopup(View anchorView, boolean isRtl,
            AutofillSuggestion[] suggestions) {

        if (mAutofillPopup == null) {
            if (WindowAndroid.activityFromContext(mContext) == null) {
                nativeDismissed(mNativeXWalkAutofillClientAndroid);
                return;
            }
            mAutofillPopup = new AutofillPopup(mContext, anchorView, new AutofillDelegate() {
                @Override
                public void dismissed() {
                    nativeDismissed(mNativeXWalkAutofillClientAndroid);
                }

                @Override
                public void suggestionSelected(int listIndex) {
                    nativeSuggestionSelected(mNativeXWalkAutofillClientAndroid, listIndex);
                }

                @Override
                public void deleteSuggestion(int listIndex) {
                }

                @Override
                public void accessibilityFocusCleared() {
                    // TODO Auto-generated method stub

                }
            });
        }
        mAutofillPopup.filterAndShow(suggestions, isRtl, false /*isRefresh*/);
    }

    @CalledByNative
    public void hideAutofillPopup() {
        if (mAutofillPopup == null)
            return;
        mAutofillPopup.dismiss();
        mAutofillPopup = null;
    }

    @CalledByNative
    private static AutofillSuggestion[] createAutofillSuggestionArray(int size) {
        return new AutofillSuggestion[size];
    }

    /**
     * @param array AutofillSuggestion array that should get a new suggestion added.
     * @param index Index in the array where to place a new suggestion.
     * @param name Name of the suggestion.
     * @param label Label of the suggestion.
     * @param uniqueId Unique suggestion id.
     */
    @CalledByNative
    private static void addToAutofillSuggestionArray(AutofillSuggestion[] array, int index,
            String name, String label, int uniqueId) {
        array[index] = new AutofillSuggestion(name, label, DropdownItem.NO_ICON,
                false /* isIconAtLeft */, uniqueId, false /* isDeletable */,
                false /* isMultilineLabel */, false /* isBoldLabel */);
    }

    private native void nativeDismissed(long nativeXWalkAutofillClientAndroid);

    private native void nativeSuggestionSelected(long nativeXWalkAutofillClientAndroid,
            int position);
}
