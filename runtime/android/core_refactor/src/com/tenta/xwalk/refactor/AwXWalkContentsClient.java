package com.tenta.xwalk.refactor;

import java.security.Principal;
import java.util.HashMap;

import org.chromium.android_webview.AwConsoleMessage;
import org.chromium.android_webview.AwContentsClient;
import org.chromium.android_webview.AwContentsClientBridge.ClientCertificateRequestCallback;
import org.chromium.android_webview.AwHttpAuthHandler;
import org.chromium.android_webview.AwRenderProcess;
import org.chromium.android_webview.AwRenderProcessGoneDetail;
import org.chromium.android_webview.AwSafeBrowsingResponse;
import org.chromium.android_webview.AwWebResourceResponse;
import org.chromium.android_webview.JsPromptResultReceiver;
import org.chromium.android_webview.JsResultReceiver;
import org.chromium.android_webview.permission.AwPermissionRequest;
import org.chromium.base.Callback;

import android.graphics.Bitmap;
import android.graphics.Picture;
import android.net.http.SslError;
import android.os.Message;
import android.view.KeyEvent;
import android.view.View;

public class AwXWalkContentsClient extends AwContentsClient {

    @Override
    public boolean hasWebViewClient() {
        // TODO Auto-generated method stub
        return false;
    }

    @Override
    public void getVisitedHistory(Callback<String[]> callback) {
        // TODO Auto-generated method stub

    }

    @Override
    public void doUpdateVisitedHistory(String url, boolean isReload) {
        // TODO Auto-generated method stub

    }

    @Override
    public void onProgressChanged(int progress) {
        // TODO Auto-generated method stub

    }

    @Override
    public AwWebResourceResponse shouldInterceptRequest(AwWebResourceRequest request) {
        // TODO Auto-generated method stub
        return null;
    }

    @Override
    public boolean shouldOverrideKeyEvent(KeyEvent event) {
        // TODO Auto-generated method stub
        return false;
    }

    @Override
    public boolean shouldOverrideUrlLoading(AwWebResourceRequest request) {
        // TODO Auto-generated method stub
        return false;
    }

    @Override
    public void onLoadResource(String url) {
        // TODO Auto-generated method stub

    }

    @Override
    public void onUnhandledKeyEvent(KeyEvent event) {
        // TODO Auto-generated method stub

    }

    @Override
    public boolean onConsoleMessage(AwConsoleMessage consoleMessage) {
        // TODO Auto-generated method stub
        return false;
    }

    @Override
    public void onReceivedHttpAuthRequest(AwHttpAuthHandler handler, String host, String realm) {
        // TODO Auto-generated method stub

    }

    @Override
    public void onReceivedSslError(Callback<Boolean> callback, SslError error) {
        // TODO Auto-generated method stub

    }

    @Override
    public void onReceivedClientCertRequest(ClientCertificateRequestCallback callback,
            String[] keyTypes, Principal[] principals, String host, int port) {
        // TODO Auto-generated method stub

    }

    @Override
    public void onReceivedLoginRequest(String realm, String account, String args) {
        // TODO Auto-generated method stub

    }

    @Override
    public void onFormResubmission(Message dontResend, Message resend) {
        // TODO Auto-generated method stub

    }

    @Override
    public void onDownloadStart(String url, String userAgent, String contentDisposition,
            String mimeType, long contentLength) {
        // TODO Auto-generated method stub

    }

    @Override
    public void showFileChooser(Callback<String[]> uploadFilePathsCallback,
            FileChooserParamsImpl fileChooserParams) {
        // TODO Auto-generated method stub

    }

    @Override
    public void onGeolocationPermissionsShowPrompt(String origin,
            org.chromium.android_webview.AwGeolocationPermissions.Callback callback) {
        // TODO Auto-generated method stub

    }

    @Override
    public void onGeolocationPermissionsHidePrompt() {
        // TODO Auto-generated method stub

    }

    @Override
    public void onPermissionRequest(AwPermissionRequest awPermissionRequest) {
        // TODO Auto-generated method stub

    }

    @Override
    public void onPermissionRequestCanceled(AwPermissionRequest awPermissionRequest) {
        // TODO Auto-generated method stub

    }

    @Override
    public void onScaleChangedScaled(float oldScale, float newScale) {
        // TODO Auto-generated method stub

    }

    @Override
    protected void handleJsAlert(String url, String message, JsResultReceiver receiver) {
        // TODO Auto-generated method stub

    }

    @Override
    protected void handleJsBeforeUnload(String url, String message, JsResultReceiver receiver) {
        // TODO Auto-generated method stub

    }

    @Override
    protected void handleJsConfirm(String url, String message, JsResultReceiver receiver) {
        // TODO Auto-generated method stub

    }

    @Override
    protected void handleJsPrompt(String url, String message, String defaultValue,
            JsPromptResultReceiver receiver) {
        // TODO Auto-generated method stub

    }

    @Override
    protected boolean onCreateWindow(boolean isDialog, boolean isUserGesture) {
        // TODO Auto-generated method stub
        return false;
    }

    @Override
    protected void onCloseWindow() {
        // TODO Auto-generated method stub

    }

    @Override
    public void onReceivedTouchIconUrl(String url, boolean precomposed) {
        // TODO Auto-generated method stub

    }

    @Override
    public void onReceivedIcon(Bitmap bitmap) {
        // TODO Auto-generated method stub

    }

    @Override
    public void onReceivedTitle(String title) {
        // TODO Auto-generated method stub

    }

    @Override
    protected void onRequestFocus() {
        // TODO Auto-generated method stub

    }

    @Override
    protected View getVideoLoadingProgressView() {
        // TODO Auto-generated method stub
        return null;
    }

    @Override
    public void onPageStarted(String url) {
        // TODO Auto-generated method stub

    }

    @Override
    public void onPageFinished(String url) {
        // TODO Auto-generated method stub

    }

    @Override
    public void onPageCommitVisible(String url) {
        // TODO Auto-generated method stub

    }

    @Override
    protected void onReceivedError(int errorCode, String description, String failingUrl) {
        // TODO Auto-generated method stub

    }

    @Override
    protected void onReceivedError2(AwWebResourceRequest request, AwWebResourceError error) {
        // TODO Auto-generated method stub

    }

    @Override
    protected void onSafeBrowsingHit(AwWebResourceRequest request, int threatType,
            Callback<AwSafeBrowsingResponse> callback) {
        // TODO Auto-generated method stub

    }

    @Override
    public void onReceivedHttpError(AwWebResourceRequest request, AwWebResourceResponse response) {
        // TODO Auto-generated method stub

    }

    @Override
    public void onShowCustomView(View view, CustomViewCallback callback) {
        // TODO Auto-generated method stub

    }

    @Override
    public void onHideCustomView() {
        // TODO Auto-generated method stub

    }

    @Override
    public Bitmap getDefaultVideoPoster() {
        // TODO Auto-generated method stub
        return null;
    }

    @Override
    public void onFindResultReceived(int activeMatchOrdinal, int numberOfMatches,
            boolean isDoneCounting) {
        // TODO Auto-generated method stub

    }

    @Override
    public void onNewPicture(Picture picture) {
        // TODO Auto-generated method stub

    }

    @Override
    public void onRendererUnresponsive(AwRenderProcess renderProcess) {
        // TODO Auto-generated method stub

    }

    @Override
    public void onRendererResponsive(AwRenderProcess renderProcess) {
        // TODO Auto-generated method stub

    }

    @Override
    public boolean onRenderProcessGone(AwRenderProcessGoneDetail detail) {
        // TODO Auto-generated method stub
        return false;
    }

    /**
     * Parameters for the {@link XWalkContentsClient#shouldInterceptRequest} method.
     */
    public static class WebResourceRequestInner {
        // Url of the request.
        public String url;
        // Is this for the main frame or a child iframe?
        public boolean isMainFrame;
        // Was a gesture associated with the request? Don't trust can easily be spoofed.
        public boolean hasUserGesture;
        // Method used (GET/POST/OPTIONS)
        public String method;
        // Headers that would have been sent to server.
        public HashMap<String, String> requestHeaders;
    }
}
