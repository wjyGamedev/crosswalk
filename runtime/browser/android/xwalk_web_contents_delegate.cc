// Copyright (c) 2013 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "xwalk/runtime/browser/android/xwalk_content.h"
#include "xwalk/runtime/browser/android/xwalk_web_contents_delegate.h"

#include <string>
#include <vector>

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/common/file_chooser_file_info.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "xwalk/runtime/android/core_refactor/xwalk_refactor_native_jni/XWalkWebContentsDelegate_jni.h"
#include "xwalk/runtime/browser/media/media_capture_devices_dispatcher.h"
//#include "xwalk/runtime/browser/runtime_file_select_helper.h"
#include "xwalk/runtime/browser/runtime_javascript_dialog_manager.h"

#include "components/viz/common/surfaces/surface_id.h"

#include "meta_logging.h"
#if defined(TENTA_CHROMIUM_BUILD)
#include "xwalk/third_party/tenta/chromium_cache/meta_cache_backend.h"
#endif

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::ConvertUTF16ToJavaString;
using base::android::ScopedJavaLocalRef;
using blink::mojom::FileChooserParams;
using content::WebContents;

namespace xwalk {

// WARNING: these constants are exposed in the public interface Java side, so
// must remain in sync with what clients are expecting.
const int kFileChooserModeOpenMultiple = 1 << 0;
const int kFileChooserModeOpenFolder = 1 << 1;

XWalkWebContentsDelegate::XWalkWebContentsDelegate(JNIEnv* env, jobject obj)
    : WebContentsDelegateAndroid(env, obj) {
}

XWalkWebContentsDelegate::~XWalkWebContentsDelegate() {
}

void XWalkWebContentsDelegate::AddNewContents(content::WebContents* source,
                                              std::unique_ptr<content::WebContents> new_contents,
                                              WindowOpenDisposition disposition,
                                              const gfx::Rect& initial_rect,
                                              bool user_gesture,
                                              bool* was_blocked) {
  JNIEnv* env = AttachCurrentThread();

  bool is_dialog = disposition == WindowOpenDisposition::NEW_POPUP;
  ScopedJavaLocalRef<jobject> java_delegate = GetJavaDelegate(env);
  bool create_popup = false;

  if (java_delegate.obj()) {
    create_popup = Java_XWalkWebContentsDelegate_addNewContents(
        env, java_delegate, is_dialog, user_gesture);
  }


  if (create_popup) {
    // The embedder would like to display the popup and we will receive
    // a callback from them later with an AwContents to use to display
    // it. The source AwContents takes ownership of the new WebContents
    // until then, and when the callback is made we will swap the WebContents
    // out into the new AwContents.
    WebContents* raw_new_contents = new_contents.get();
    XWalkContent::FromWebContents(source)->SetPendingWebContentsForPopup(
        std::move(new_contents));
    // It's possible that SetPendingWebContentsForPopup deletes |new_contents|,
    // but it only does so asynchronously, so it's safe to use a raw pointer
    // here.
    // Hide the WebContents for the pop up now, we will show it again
    // when the user calls us back with an AwContents to use to show it.
    raw_new_contents->WasHidden();
  } else {
    // The embedder has forgone their chance to display this popup
    // window, so we're done with the WebContents now. We use
    // DeleteSoon as WebContentsImpl may call methods on |new_contents|
    // after this method returns.
    base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE,
                                                    std::move(new_contents));
  }

  if (was_blocked) {
    *was_blocked = !create_popup;
  }
}

void XWalkWebContentsDelegate::CloseContents(content::WebContents* source) {
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jobject> java_delegate = GetJavaDelegate(env);
  if (java_delegate.obj()) {
    Java_XWalkWebContentsDelegate_closeContents(env, java_delegate);
  }
}

void XWalkWebContentsDelegate::ActivateContents(content::WebContents* source) {
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jobject> java_delegate = GetJavaDelegate(env);
  if (java_delegate.obj()) {
    Java_XWalkWebContentsDelegate_activateContents(env, java_delegate);
  }
}

void XWalkWebContentsDelegate::UpdatePreferredSize(content::WebContents* contents, const gfx::Size& pref_size) {
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jobject> java_delegate = GetJavaDelegate(env);
  if (java_delegate.obj()) {
    Java_XWalkWebContentsDelegate_updatePreferredSize(env, java_delegate,
                                                      pref_size.width(),
                                                      pref_size.height());
  }
}

void XWalkWebContentsDelegate::RunFileChooser(content::RenderFrameHost* render_frame_host,
                                              std::unique_ptr<content::FileSelectListener> listener,
                                              const blink::mojom::FileChooserParams& params) {
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jobject> java_delegate = GetJavaDelegate(env);
  if (!java_delegate.obj()) {
    listener->FileSelectionCanceled();
    return;
  }

  int mode_flags = 0;
  if (params.mode == FileChooserParams::Mode::kOpenMultiple) {
    mode_flags |= kFileChooserModeOpenMultiple;
  } else if (params.mode == FileChooserParams::Mode::kUploadFolder) {
    // Folder implies multiple in Chrome.
    mode_flags |= kFileChooserModeOpenMultiple | kFileChooserModeOpenFolder;
  } else if (params.mode == FileChooserParams::Mode::kSave) {
    // Save not supported, so cancel it.
    listener->FileSelectionCanceled();
    return;
  } else {
    DCHECK_EQ(FileChooserParams::Mode::kOpen, params.mode);
  }
  DCHECK(!file_select_listener_)
        << "Multiple concurrent FileChooser requests are not supported.";

  file_select_listener_ = std::move(listener);

  jboolean result = Java_XWalkWebContentsDelegate_shouldOverrideRunFileChooser(
      env, java_delegate,
      render_frame_host->GetProcess()->GetID(),
      render_frame_host->GetRoutingID(),
      mode_flags,
      ConvertUTF16ToJavaString(env, base::JoinString(params.accept_types, base::ASCIIToUTF16(","))),
      params.title.empty() ? nullptr : ConvertUTF16ToJavaString(env, params.title),
      params.default_file_name.empty() ? nullptr : ConvertUTF8ToJavaString(env, params.default_file_name.value()),
      params.use_media_capture);

  if (!result) {
    listener->FileSelectionCanceled();
  }
}

std::unique_ptr<content::FileSelectListener>
XWalkWebContentsDelegate::TakeFileSelectListener() {
  return std::move(file_select_listener_);
}

content::JavaScriptDialogManager*
XWalkWebContentsDelegate::GetJavaScriptDialogManager(WebContents* source) {
  if (!javascript_dialog_manager_.get()) {
    javascript_dialog_manager_.reset(new RuntimeJavaScriptDialogManager);
  }
  return javascript_dialog_manager_.get();
}

void XWalkWebContentsDelegate::RequestMediaAccessPermission(content::WebContents* web_contents,
                                                            const content::MediaStreamRequest& request,
                                                            content::MediaResponseCallback callback) {

  LOG(INFO) << "XWalkWebContentsDelegate::RequestMediaAccessPermission";

  XWalkMediaCaptureDevicesDispatcher::RunRequestMediaAccessPermission(web_contents, request, std::move(callback));

//  AwContents* aw_contents = AwContents::FromWebContents(web_contents);
//  if (!aw_contents) {
//    std::move(callback).Run(
//        blink::MediaStreamDevices(),
//        blink::mojom::MediaStreamRequestResult::FAILED_DUE_TO_SHUTDOWN,
//        std::unique_ptr<content::MediaStreamUI>());
//    return;
//  }
//  aw_contents->GetPermissionRequestHandler()->SendRequest(
//      std::unique_ptr<AwPermissionRequestDelegate>(
//          new MediaAccessPermissionRequest(request, std::move(callback))));
}
/*
 *
 void TabWebContentsDelegateAndroid::RequestMediaAccessPermission(
 content::WebContents* web_contents,
 const content::MediaStreamRequest& request,
 const content::MediaResponseCallback& callback) {
 MediaCaptureDevicesDispatcher::GetInstance()->ProcessMediaAccessRequest(
 web_contents, request, callback, NULL);
 }

 bool TabWebContentsDelegateAndroid::CheckMediaAccessPermission(
 content::WebContents* web_contents,
 const GURL& security_origin,
 content::MediaStreamType type) {
 return MediaCaptureDevicesDispatcher::GetInstance()
 ->CheckMediaAccessPermission(web_contents, security_origin, type);
 }
 */

void XWalkWebContentsDelegate::RendererUnresponsive(content::WebContents* source,
                                                    content::RenderWidgetHost* render_widget_host,
                                                    base::RepeatingClosure hang_monitor_restarter) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return;

  content::RenderProcessHost* render_process_host =
      render_widget_host->GetProcess();
  if (render_process_host->IsInitializedAndNotDead()) {
    Java_XWalkWebContentsDelegate_rendererUnresponsive(env, obj);
    hang_monitor_restarter.Run();
  }
}

void XWalkWebContentsDelegate::RendererResponsive(content::WebContents* source,
                                                  content::RenderWidgetHost* render_widget_host) {

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return;

  content::RenderProcessHost* render_process_host = render_widget_host->GetProcess();
  if (render_process_host->IsInitializedAndNotDead()) {
    Java_XWalkWebContentsDelegate_rendererResponsive(env, obj);
  }
}

bool XWalkWebContentsDelegate::DidAddMessageToConsole(content::WebContents* source,
                                                      blink::mojom::ConsoleMessageLevel log_level,
                                                      const base::string16& message,
                                                      int32_t line_no,
                                                      const base::string16& source_id) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return WebContentsDelegate::DidAddMessageToConsole(source, log_level, message, line_no, source_id);
  ScopedJavaLocalRef<jstring> jmessage(ConvertUTF16ToJavaString(env, message));
  ScopedJavaLocalRef<jstring> jsource_id(ConvertUTF16ToJavaString(env, source_id));
  int jlevel = web_contents_delegate_android::WEB_CONTENTS_DELEGATE_LOG_LEVEL_DEBUG;
  switch (log_level) {
    case blink::mojom::ConsoleMessageLevel::kVerbose:
      jlevel = web_contents_delegate_android::WEB_CONTENTS_DELEGATE_LOG_LEVEL_DEBUG;
      break;
    case blink::mojom::ConsoleMessageLevel::kInfo:
      jlevel = web_contents_delegate_android::WEB_CONTENTS_DELEGATE_LOG_LEVEL_LOG;
      break;
    case blink::mojom::ConsoleMessageLevel::kWarning:
      jlevel = web_contents_delegate_android::WEB_CONTENTS_DELEGATE_LOG_LEVEL_WARNING;
      break;
    case blink::mojom::ConsoleMessageLevel::kError:
      jlevel = web_contents_delegate_android::WEB_CONTENTS_DELEGATE_LOG_LEVEL_ERROR;
      break;
    default:
      NOTREACHED();
  }
  return Java_XWalkWebContentsDelegate_addMessageToConsole(env, GetJavaDelegate(env), jlevel, jmessage, line_no,
                                                           jsource_id);
}

bool XWalkWebContentsDelegate::HandleKeyboardEvent(content::WebContents* source,
                                                   const content::NativeWebKeyboardEvent& event) {
  ScopedJavaGlobalRef<jobject> key_event = event.os_event;
  if (!key_event.obj())
    return true;
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return true;
  Java_XWalkWebContentsDelegate_handleKeyboardEvent(env, obj, key_event);
  return true;
}

void XWalkWebContentsDelegate::ShowRepostFormWarningDialog(WebContents* source) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return;
  Java_XWalkWebContentsDelegate_showRepostFormWarningDialog(env, obj);
}

void XWalkWebContentsDelegate::EnterFullscreenModeForTab(content::WebContents* web_contents, const GURL& origin,
                                                         const blink::WebFullscreenOptions& options) {
  LOG(INFO) << "iotto " << __func__;
//  JNIEnv* env = AttachCurrentThread();
//  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
//  if (obj.is_null())
//    return;
//  Java_XWalkWebContentsDelegate_toggleFullscreen(env, obj, true);
  WebContentsDelegateAndroid::EnterFullscreenModeForTab(web_contents, origin,
                                                        options);
  web_contents->GetRenderViewHost()->GetWidget()->SynchronizeVisualProperties();
}

void XWalkWebContentsDelegate::ExitFullscreenModeForTab(content::WebContents* web_contents) {
  LOG(INFO) << "iotto " << __func__;
//  JNIEnv* env = AttachCurrentThread();
//  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
//  if (obj.is_null())
//    return;
//  Java_XWalkWebContentsDelegate_toggleFullscreen(env, obj, false);
  WebContentsDelegateAndroid::ExitFullscreenModeForTab(web_contents);
  web_contents->GetRenderViewHost()->GetWidget()->SynchronizeVisualProperties();
}

bool XWalkWebContentsDelegate::IsFullscreenForTabOrPending(const content::WebContents* web_contents) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return false;
  return Java_XWalkWebContentsDelegate_isFullscreen(env, obj);
}

bool XWalkWebContentsDelegate::ShouldCreateWebContents(
    content::WebContents* web_contents, content::RenderFrameHost* opener,
    content::SiteInstance* source_site_instance, int32_t route_id,
    int32_t main_frame_route_id, int32_t main_frame_widget_route_id,
    content::mojom::WindowContainerType window_container_type,
    const GURL& opener_url, const std::string& frame_name,
    const GURL& target_url, const std::string& partition_id,
    content::SessionStorageNamespace* session_storage_namespace) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return true;
  ScopedJavaLocalRef<jstring> java_url = ConvertUTF8ToJavaString(
      env, target_url.spec());
  return Java_XWalkWebContentsDelegate_shouldCreateWebContents(env, obj,
                                                               java_url);
}

void XWalkWebContentsDelegate::FindReply(WebContents* web_contents, int request_id, int number_of_matches,
                                         const gfx::Rect& selection_rect, int active_match_ordinal,
                                         bool final_update) {
  XWalkContent* xwalk_content = XWalkContent::FromWebContents(web_contents);
  if (!xwalk_content)
    return;

  xwalk_content->GetFindHelper()->HandleFindReply(request_id, number_of_matches, active_match_ordinal, final_update);
}

/**
 *
 */
void XWalkWebContentsDelegate::NavigationStateChanged(content::WebContents* source,
                                                      content::InvalidateTypes changed_flags) {
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jobject> java_delegate = GetJavaDelegate(env);
  if (java_delegate.obj()) {
    Java_XWalkWebContentsDelegate_navigationStateChanged(env, java_delegate, changed_flags);
  } else {
    TENTA_LOG_CACHE(ERROR) << __func__ << " null_java_delegate";
  }
#if defined(TENTA_CHROMIUM_BUILD)
  TENTA_LOG_CACHE(INFO) << __func__ << " changed_flags=" << changed_flags << " url=" << source->GetLastCommittedURL();
  ::tenta::fs::cache::MetaCacheBackend* backend = ::tenta::fs::cache::MetaCacheBackend::GetInstance();
  if ( changed_flags == content::InvalidateTypes::INVALIDATE_TYPE_ALL) {
    // start all over
    backend->DropBuffer();
  }
#endif
}

/**
 *
 */
void XWalkWebContentsDelegate::LoadingStateChanged(content::WebContents* source, bool to_different_document) {
#if defined(TENTA_CHROMIUM_BUILD)
  TENTA_LOG_CACHE(INFO) << __func__ << " to_different_document=" << to_different_document << " is_loading="
                        << source->IsLoading();
#endif
}

bool XWalkWebContentsDelegate::ShouldResumeRequestsForCreatedWindow() {
  TENTA_LOG(INFO) << "iotto " << __func__;
  // Always return false here since we need to defer loading the created window
  // until after we have attached a new delegate to the new webcontents (which
  // happens asynchronously).
  return false;
}

void XWalkWebContentsDelegate::SetOverlayMode(bool useOverlayMode) {
  LOG(INFO) << "iotto " << __func__ << " mode=" << useOverlayMode;
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jobject> java_delegate = GetJavaDelegate(env);
  if (java_delegate.obj()) {
    Java_XWalkWebContentsDelegate_setOverlayMode(env, java_delegate, useOverlayMode);
  } else {
    TENTA_LOG_CACHE(ERROR) << __func__ << " null_java_delegate";
  }
}

content::PictureInPictureResult XWalkWebContentsDelegate::EnterPictureInPicture(content::WebContents* web_contents,
                                                                                const viz::SurfaceId& surfaceId,
                                                                                const gfx::Size& natural_size) {

  LOG(INFO) << "iotto " << __func__ << " surface=" << surfaceId.ToString();
  return content::PictureInPictureResult::kSuccess;
}

bool RegisterXWalkWebContentsDelegate(JNIEnv* env) {
  return true;
}

}  // namespace xwalk
