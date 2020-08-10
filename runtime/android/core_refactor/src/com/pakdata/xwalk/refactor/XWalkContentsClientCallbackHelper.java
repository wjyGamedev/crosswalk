// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.pakdata.xwalk.refactor;

import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.util.Log;

/**
 * This class is responsible for calling certain client callbacks on the UI thread.
 *
 * Most callbacks do no go through here, but get forwarded to XWalkContentsClient directly. The
 * messages processed here may originate from the IO or UI thread.
 */
class XWalkContentsClientCallbackHelper {

    private static class OnReceivedErrorInfo {
        final int mErrorCode;
        final String mDescription;
        final String mFailingUrl;

        OnReceivedErrorInfo(int errorCode, String description, String failingUrl) {
            mErrorCode = errorCode;
            mDescription = description;
            mFailingUrl = failingUrl;
        }
    }

    private static class OnReceivedResponseHeadersInfo {
        final XWalkContentsClient.XWalkWebResourceRequest mRequest;
        final XWalkWebResourceResponse mResponse;

        OnReceivedResponseHeadersInfo(
                XWalkContentsClient.XWalkWebResourceRequest request,
                XWalkWebResourceResponse response) {
            mRequest = request;
            mResponse = response;
        }
    }

    private final static int MSG_ON_LOAD_RESOURCE = 1;
    private final static int MSG_ON_PAGE_STARTED = 2;
    private final static int MSG_ON_RECEIVED_ERROR = 5;
    private final static int MSG_ON_RESOURCE_LOAD_STARTED = 6;
    private final static int MSG_ON_PAGE_FINISHED = 7;
    private final static int MSG_ON_RECEIVED_RESPONSE_HEADERS = 8;
    private final static int MSG_SYNTHESIZE_PAGE_LOADING = 9;

    private final XWalkContentsClient mContentsClient;

    private final Handler mHandler = new Handler(Looper.getMainLooper()) {
        @Override
        public void handleMessage(Message msg) {
            switch(msg.what) {
                case MSG_ON_LOAD_RESOURCE: {
                    final String url = (String) msg.obj;
                    mContentsClient.onLoadResource(url);
                    break;
                }
                case MSG_ON_PAGE_STARTED: {
                    final String url = (String) msg.obj;
                    mContentsClient.onPageStarted(url);
                    break;
                }
                case MSG_ON_RECEIVED_ERROR: {
                    OnReceivedErrorInfo info = (OnReceivedErrorInfo) msg.obj;
                    mContentsClient.onReceivedError(info.mErrorCode, info.mDescription,
                            info.mFailingUrl);
                    break;
                }
                case MSG_ON_RESOURCE_LOAD_STARTED: {
                    final String url = (String) msg.obj;
                    mContentsClient.onResourceLoadStarted(url);
                    break;
                }
                case MSG_ON_PAGE_FINISHED: {
                    final String url = (String) msg.obj;
                    mContentsClient.onPageFinished(url);
                    break;
                }
                case MSG_SYNTHESIZE_PAGE_LOADING: {
                    final String url = (String) msg.obj;
                    mContentsClient.onPageStarted(url);
                    mContentsClient.onLoadResource(url);
                    mContentsClient.onProgressChanged(100);
                    mContentsClient.onPageFinished(url);
                    break;
                }
                case MSG_ON_RECEIVED_RESPONSE_HEADERS: {
                    OnReceivedResponseHeadersInfo info = (OnReceivedResponseHeadersInfo) msg.obj;
                    mContentsClient.onReceivedResponseHeaders(info.mRequest, info.mResponse);
                    break;
                }
                default:
                    throw new IllegalStateException(
                            "XWalkContentsClientCallbackHelper: unhandled message " + msg.what);
            }
        }
    };

    public XWalkContentsClientCallbackHelper(XWalkContentsClient contentsClient) {
        mContentsClient = contentsClient;
    }

    public void postOnLoadResource(String url) {
        mHandler.sendMessage(mHandler.obtainMessage(MSG_ON_LOAD_RESOURCE, url));
    }

    public void postSynthesizedPageLoadingForUrlBarUpdate(String url) {
        mHandler.sendMessage(mHandler.obtainMessage(MSG_SYNTHESIZE_PAGE_LOADING, url));
    }
    
    public void postOnPageStarted(String url) {
        mHandler.sendMessage(mHandler.obtainMessage(MSG_ON_PAGE_STARTED, url));
    }

    public void postOnReceivedError(int errorCode, String description, String failingUrl) {
        OnReceivedErrorInfo info = new OnReceivedErrorInfo(errorCode, description, failingUrl);
        mHandler.sendMessage(mHandler.obtainMessage(MSG_ON_RECEIVED_ERROR, info));
    }

    public void postOnResourceLoadStarted(String url) {
        mHandler.sendMessage(mHandler.obtainMessage(MSG_ON_RESOURCE_LOAD_STARTED, url));
    }

    public void postOnPageFinished(String url) {
        mHandler.sendMessage(mHandler.obtainMessage(MSG_ON_PAGE_FINISHED, url));
    }

    public void postOnReceivedResponseHeaders(XWalkContentsClient.XWalkWebResourceRequest request,
            XWalkWebResourceResponse response) {
        OnReceivedResponseHeadersInfo info =
                new OnReceivedResponseHeadersInfo(request, response);
        mHandler.sendMessage(mHandler.obtainMessage(MSG_ON_RECEIVED_RESPONSE_HEADERS, info));
    }
}
