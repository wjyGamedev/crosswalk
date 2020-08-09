// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Copyright (c) 2013 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef XWALK_RUNTIME_BROWSER_ANDROID_XWALK_CONTENT_H_
#define XWALK_RUNTIME_BROWSER_ANDROID_XWALK_CONTENT_H_

#include <list>
#include <memory>
#include <utility>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"
#include "xwalk/runtime/browser/android/find_helper.h"
#include "xwalk/runtime/browser/android/renderer_host/xwalk_render_view_host_ext.h"
#include "third_party/skia/include/core/SkBitmap.h"

#ifdef TENTA_CHROMIUM_BUILD
#include "browser/neterror/tenta_net_error_client.h"
#include "xwalk/third_party/tenta/meta_fs/jni/meta_virtual_file.h"
using tenta::fs::MetaFile;

namespace tenta {
namespace fs {
class MetaDb;
class MetaFile;
}  // namespace fs
} 

#endif

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace content {
class BrowserContext;
class WebContents;
}

namespace xwalk {
namespace tenta {
class FsDelegateSqlite;
}
class XWalkAutofillManager;
class XWalkWebContentsDelegate;
class XWalkContentsClientBridge;
class JsJavaConfiguratorHost;

class XWalkContent :
    public FindHelper::Listener
#ifdef TENTA_CHROMIUM_BUILD
    ,
    public ::tenta::ext::TentaNetErrorClient::Listener
#endif
{
 public:
  static std::string GetLocale();
  static std::string GetLocaleList();
  static XWalkContent* FromID(int render_process_id, int render_view_id);
  static XWalkContent* FromWebContents(content::WebContents* web_contents);

  explicit XWalkContent(std::unique_ptr<content::WebContents> web_contents);
  ~XWalkContent() override;

  base::android::ScopedJavaLocalRef<jobject> GetWebContents(JNIEnv* env, jobject obj);
  void SetPendingWebContentsForPopup(std::unique_ptr<content::WebContents> pending);
  jlong ReleasePopupXWalkContent(JNIEnv* env, jobject obj);
  void SetJavaPeers(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& xwalk_content,
      const base::android::JavaParamRef<jobject>& web_contents_delegate,
      const base::android::JavaParamRef<jobject>& contents_client_bridge,
      const base::android::JavaParamRef<jobject>& io_thread_client,
      const base::android::JavaParamRef<jobject>& intercept_navigation_delegate);
  void ClearCache(JNIEnv* env, jobject obj, jboolean include_disk_files);
  void ClearCacheForSingleFile(JNIEnv* env, jobject obj, jstring url);
  ScopedJavaLocalRef<jstring> DevToolsAgentId(JNIEnv* env, jobject obj);
  void Destroy(JNIEnv* env, jobject obj);
  void UpdateLastHitTestData(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void RequestNewHitTestDataAt(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj, jfloat x, jfloat y,
                               jfloat touch_major);
  ScopedJavaLocalRef<jstring> GetVersion(JNIEnv* env, jobject obj);
  jint GetRoutingID(JNIEnv* env, jobject obj);
  base::android::ScopedJavaLocalRef<jbyteArray> GetState(JNIEnv* env, jobject obj);
  jboolean SetState(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj,
                    const base::android::JavaParamRef<jbyteArray>& state);

  /******** Using Metafs **********/
  //TODO make this private
#ifdef TENTA_CHROMIUM_BUILD
  int OpenHistoryFile(JNIEnv* env, const JavaParamRef<jstring>& id, const JavaParamRef<jstring>& key,
                      scoped_refptr<::tenta::fs::MetaFile>& fileOut, scoped_refptr<::tenta::fs::MetaDb>& dbOut,
                      int mode);
#endif

  jint SaveOldHistory(JNIEnv* env, const JavaParamRef<jobject>& obj, const JavaParamRef<jbyteArray>& state,
                      const JavaParamRef<jstring>& id, const JavaParamRef<jstring>& key);

  jint SaveHistory(JNIEnv* env, const JavaParamRef<jobject>& obj, const JavaParamRef<jstring>& id,
                   const JavaParamRef<jstring>& key);

  jint RestoreHistory(JNIEnv* env, const JavaParamRef<jobject>& obj, const JavaParamRef<jstring>& id,
                      const JavaParamRef<jstring>& key);

  jint NukeHistory(JNIEnv* env, const JavaParamRef<jobject>& obj, const JavaParamRef<jstring>& id,
                   const JavaParamRef<jstring>& key);

  jint ReKeyHistory(JNIEnv* env, const JavaParamRef<jobject>& obj, const JavaParamRef<jstring>& oldKey,
                    const JavaParamRef<jstring>& newKey);

  /******** End Metafs **********/

//  jboolean PushStateWitkKey(JNIEnv* env, jobject obj,
//                            const JavaParamRef<jbyteArray>& state,
//                            jstring id,
//                            jstring key);
//  // TODO make private
//  bool SaveArrayToDb(const char * data, int data_len, JNIEnv* env, jstring id,
//                     jstring key);
//
//  jboolean SaveStateWithKey(JNIEnv* env, jobject obj, jstring id, jstring key);
//  jboolean RestoreStateWithKey(JNIEnv* env, jobject obj, jstring id,
//                               jstring key);
//  jboolean NukeStateWithKey(JNIEnv* env, jobject obj, jstring id, jstring key);
//  jboolean RekeyStateWithKey(JNIEnv* env, jobject obj, jstring oldKey, jstring newKey);
//  /**
//   * Fill |out| with app path + history.db
//   * @param out storage for file+path storage
//   * @return true if success; false otherwise
//   */
//  bool GetHistoryDbPath(std::string& out);
  XWalkRenderViewHostExt* render_view_host_ext() {
    return render_view_host_ext_.get();
  }

  XWalkContentsClientBridge* GetContentsClientBridge() {
    return contents_client_bridge_.get();
  }

  void SetJsOnlineProperty(JNIEnv* env, jobject obj, jboolean network_up);
  jboolean SetManifest(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj,
                       const base::android::JavaParamRef<jstring>& path,
                       const base::android::JavaParamRef<jstring>& manifest);
  void SetBackgroundColor(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj, jint color);
  void SetOriginAccessWhitelist(JNIEnv* env, jobject obj, jstring url, jstring match_patterns);

  // Geolocation API support
  void ShowGeolocationPrompt(const GURL& origin, base::OnceCallback<void(bool)> callback);  // NOLINT
  void HideGeolocationPrompt(const GURL& origin);
  void InvokeGeolocationCallback(JNIEnv* env, jobject obj, jboolean value, jstring origin);

  void SetXWalkAutofillClient(const base::android::JavaRef<jobject>& client);
  void SetSaveFormData(bool enabled);

  base::android::ScopedJavaLocalRef<jbyteArray> GetCertificate(JNIEnv* env, const JavaParamRef<jobject>& obj);

  base::android::ScopedJavaLocalRef<jobjectArray> GetCertificateChain(JNIEnv* env, const JavaParamRef<jobject>& obj);

  FindHelper* GetFindHelper();
  void FindAllAsync(JNIEnv* env, const JavaParamRef<jobject>& obj, const JavaParamRef<jstring>& search_string);
  void FindNext(JNIEnv* env, const JavaParamRef<jobject>& obj, jboolean forward);
  void ClearMatches(JNIEnv* env, const JavaParamRef<jobject>& obj);
  void CaptureBitmapWithParams(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj, float scale,
                               const base::android::JavaParamRef<jobject>& callback);

  // FindHelper::Listener implementation.
  void OnFindResultReceived(int active_ordinal, int match_count, bool finished) override;

 private:
  void OnDidCaptureBitmap(const base::android::JavaRef<jobject>& obj, const base::android::JavaRef<jobject>& callback,
                          float scale, const SkBitmap& bitmap);

  JsJavaConfiguratorHost* GetJsJavaConfiguratorHost();

#ifdef TENTA_CHROMIUM_BUILD
  // TentaNetErrorClient::Listener
  void OnOpenDnsSettings(const GURL& failedUrl) override;
#else
  void OnOpenDnsSettings(const GURL& failedUrl);
#endif

 private:
  JavaObjectWeakGlobalRef java_ref_;
  // TODO(guangzhen): The WebContentsDelegate need to take ownership of
  // WebContents as chrome content design. For xwalk, XWalkContent owns
  // these two, we need to redesign XWalkContent in the future.
  // Currently as a workaround, below declaration order makes sure
  // the WebContents destructed before WebContentsDelegate.
  std::unique_ptr<XWalkWebContentsDelegate> web_contents_delegate_;
  std::unique_ptr<XWalkRenderViewHostExt> render_view_host_ext_;
  std::unique_ptr<XWalkContentsClientBridge> contents_client_bridge_;
  std::unique_ptr<XWalkAutofillManager> xwalk_autofill_manager_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<XWalkContent> pending_contents_;
  std::unique_ptr<FindHelper> find_helper_;
  std::unique_ptr<JsJavaConfiguratorHost> js_java_configurator_host_;

  // GURL is supplied by the content layer as requesting frame.
  // Callback is supplied by the content layer, and is invoked with the result
  // from the permission prompt.
  typedef std::pair<const GURL, base::OnceCallback<void(bool)> > /* NOLINT */
  OriginCallback;
  // The first element in the list is always the currently pending request.
  std::list<OriginCallback> pending_geolocation_prompts_;

  int _zone_id;
  int _tab_id; // curent webview ID
  base::WeakPtrFactory<XWalkContent> _weak_ptr_factory {this};
};

}  // namespace xwalk

#endif  // XWALK_RUNTIME_BROWSER_ANDROID_XWALK_CONTENT_H_
