/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.tenta.xwalk.refactor;

import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.graphics.Bitmap;
import android.net.http.SslError;
import android.os.Message;
import android.view.KeyEvent;
import android.webkit.ValueCallback;
import android.webkit.WebResourceResponse;
import android.widget.FrameLayout;
import android.widget.TextView;

/**
 * It's the Internal class to handle legacy resource related callbacks not
 * handled by XWalkResourceClientInternal.
 *
 * @hide
 */
public class XWalkClient {

    private Context mContext;
    private XWalkView mXWalkView;

    public XWalkClient(XWalkView view) {
        mContext = view.getContext();
        mXWalkView = view;
    }

    /**
     * Notify the host application that the renderer of XWalkView is hung.
     *
     * @param view The XWalkView on which the render is hung.
     */
    public void onRendererUnresponsive(XWalkView view) {
    }

    /**
     * Notify the host application that the renderer of XWalkView is no longer hung.
     *
     * @param view The XWalkView which becomes responsive now.
     */
    public void onRendererResponsive(XWalkView view) {
    }

    /**
     * Notify the host application that there have been an excessive number of
     * HTTP redirects. As the host application if it would like to continue
     * trying to load the resource. The default behavior is to send the cancel
     * message.
     *
     * @param view The XWalkView that is initiating the callback.
     * @param cancelMsg The message to send if the host wants to cancel
     * @param continueMsg The message to send if the host wants to continue
     * @deprecated This method is no longer called. When the XWalkView encounters
     *             a redirect loop, it will cancel the load.
     */
    @Deprecated
    public void onTooManyRedirects(XWalkView view, Message cancelMsg,
            Message continueMsg) {
        cancelMsg.sendToTarget();
    }

    /**
     * As the host application if the browser should resend data as the
     * requested page was a result of a POST. The default is to not resend the
     * data.
     *
     * @param view The XWalkView that is initiating the callback.
     * @param dontResend The message to send if the browser should not resend
     * @param resend The message to send if the browser should resend data
     */
    public void onFormResubmission(XWalkView view, Message dontResend,
            Message resend) {
        dontResend.sendToTarget();
    }

    /**
     * Notify the host application to update its visited links database.
     *
     * @param view The XWalkView that is initiating the callback.
     * @param url The url being visited.
     * @param isReload True if this url is being reloaded.
     */
    public void doUpdateVisitedHistory(XWalkView view, String url,
            boolean isReload) {
    }

    /**
     * Notify the host application that an SSL error occurred while loading a
     * resource, but the XWalkView chose to proceed anyway based on a
     * decision retained from a previous response to onReceivedSslError().
     * @hide
     */
    public void onProceededAfterSslError(XWalkView view, SslError error) {
    }

    /**
     * Notify the host application to handle a SSL client certificate
     * request (display the request to the user and ask whether to
     * proceed with a client certificate or not). The host application
     * has to call either handler.cancel() or handler.proceed() as the
     * connection is suspended and waiting for the response. The
     * default behavior is to cancel, returning no client certificate.
     *
     * @param view The XWalkView that is initiating the callback.
     * @param handler A ClientCertRequestHandler object that will
     *            handle the user's response.
     * @param host_and_port The host and port of the requesting server.
     *
     * @hide
     */
    // TODO: comment this method temporarily, will implemtent later when all
    //       dependencies are resovled.
    // public void onReceivedClientCertRequest(XWalkView view,
    //         ClientCertRequestHandler handler, String host_and_port) {
    //     handler.cancel();
    // }

    /**
     * Notify the host application that a request to automatically log in the
     * user has been processed.
     * @param view The XWalkView requesting the login.
     * @param realm The account realm used to look up accounts.
     * @param account An optional account. If not null, the account should be
     *                checked against accounts on the device. If it is a valid
     *                account, it should be used to log in the user.
     * @param args Authenticator specific arguments used to log in the user.
     */
    public void onReceivedLoginRequest(XWalkView view, String realm,
            String account, String args) {
    }

    // TODO(yongsheng): legacy method. Consider removing it?
    public void onLoadResource(XWalkView view, String url) {
    }
}
