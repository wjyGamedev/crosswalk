// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Copyright (c) 2013 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "xwalk/runtime/browser/android/xwalk_content.h"

#include <memory>
#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

#include "base/android/callback_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/locale_utils.h"
#include "base/base_paths_android.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/pickle.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "components/navigation_interception/intercept_navigation_delegate.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/geometry/rect_f.h"
#include "xwalk/application/common/application_manifest_constants.h"
#include "xwalk/application/common/manifest.h"
#include "xwalk/runtime/android/core_refactor/xwalk_refactor_native_jni/XWalkContent_jni.h"
#include "xwalk/runtime/browser/android/js_java_interaction/js_java_configurator_host.h"
#include "xwalk/runtime/browser/android/net_disk_cache_remover.h"
#include "xwalk/runtime/browser/android/state_serializer.h"
#include "xwalk/runtime/browser/android/xwalk_autofill_client_android.h"
#include "xwalk/runtime/browser/android/xwalk_content_lifecycle_notifier.h"
#include "xwalk/runtime/browser/android/xwalk_contents_client_bridge.h"
#include "xwalk/runtime/browser/android/xwalk_contents_io_thread_client.h"
#include "xwalk/runtime/browser/android/xwalk_web_contents_delegate.h"
#include "xwalk/runtime/browser/runtime_resource_dispatcher_host_delegate_android.h"
#include "xwalk/runtime/browser/xwalk_autofill_manager.h"
#include "xwalk/runtime/browser/xwalk_browser_context.h"
#include "xwalk/runtime/browser/xwalk_runner.h"

#ifdef TENTA_CHROMIUM_BUILD
#include "xwalk/third_party/tenta/meta_fs/meta_errors.h"
#include "xwalk/third_party/tenta/meta_fs/meta_db.h"
#include "xwalk/third_party/tenta/meta_fs/meta_file.h"
#include "xwalk/third_party/tenta/meta_fs/a_cancellable_read_listener.h"
#include "xwalk/third_party/tenta/meta_fs/jni/meta_fs_manager.h"
#include "xwalk/third_party/tenta/meta_fs/jni/meta_file_system.h"
#include "xwalk/third_party/tenta/meta_fs/jni/meta_virtual_file.h"
#include "xwalk/third_party/tenta/meta_fs/jni/java_byte_array.h"
#include "xwalk/third_party/tenta/chromium_cache/meta_cache_backend.h"

#include "tenta_history_store.h"
#include "browser/tenta_tab_model.h"
#include "browser/tenta_tab_history.h"
#include "common/tenta_zone_tab_id.h"

using namespace tenta::ext;
namespace metafs = ::tenta::fs;
#endif  // #ifdef TENTA_CHROMIUM_BUILD

#include "meta_logging.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::ConvertUTF16ToJavaString;
using base::android::ScopedJavaLocalRef;
using content::BrowserThread;
using content::WebContents;
using navigation_interception::InterceptNavigationDelegate;
using xwalk::application_manifest_keys::kDisplay;

namespace keys = xwalk::application_manifest_keys;

namespace xwalk {

namespace {
std::string* g_locale() {
  static base::NoDestructor<std::string> locale;
  return locale.get();
}

std::string* g_locale_list() {
  static base::NoDestructor<std::string> locale_list;
  return locale_list.get();
}

const int cBlkSize = 4 * 1024;

const void* kXWalkContentUserDataKey = &kXWalkContentUserDataKey;

#ifdef TENTA_CHROMIUM_BUILD
// database name for history
static const std::string cHistoryDb = "c22b0c42-90d5-4a1e-9565-79cbab3b22dd";
static const int cHistoryBlockSize = ::tenta::fs::MetaBlockInfo::BLOCK_64K;


// TODO make a template AutoCallClose
/**
 *
 */
class AutoCloseMetaFile {
 public:
  AutoCloseMetaFile(scoped_refptr<metafs::MetaFile>& mvf)
      : _mvf(mvf) {
  }
  ~AutoCloseMetaFile() {
    if (_mvf.get() != nullptr) {
      _mvf->Close();
    }
  }
 private:
  scoped_refptr<metafs::MetaFile>& _mvf;
};

class AutoCloseMetaDb {
 public:
  AutoCloseMetaDb(scoped_refptr<metafs::MetaDb>& db)
      : _db(db) {
  }
  ~AutoCloseMetaDb() {
    if (_db.get() != nullptr) {
      ::tenta::fs::MetaFsManager * mng = ::tenta::fs::MetaFsManager::GetInstance();
      mng->CloseDb(cHistoryDb);
    }
  }
 private:
  scoped_refptr<metafs::MetaDb>& _db;
};

/**
 *
 */
class MetaReadToPickle :
    public metafs::ACancellableReadListener {
 public:
  MetaReadToPickle(base::Pickle& pickle)
      : ACancellableReadListener(ScopedJavaGlobalRef<jobject>()),
        _pickle_to_fill(pickle) {

  }

  void OnData(const char *data, int length) override {
    _pickle_to_fill.WriteBytes(data, length);
  }
  void OnStart(const std::string &guid, int length) override {
  }
  void OnProgress(float percent) override {
  }
  void OnDone(int status) override {
  }
  bool wasCancelledNotify(int* outStatus = nullptr) override {
    return false;
  }
 private:
  base::Pickle& _pickle_to_fill;
};

#endif // TENTA_CHROMIUM_BUILD
/**
 *
 */
class XWalkContentUserData :
    public base::SupportsUserData::Data {
 public:
  explicit XWalkContentUserData(XWalkContent* ptr)
      : content_(ptr) {
  }
  static XWalkContent* GetContents(content::WebContents* web_contents) {
    if (!web_contents)
      return nullptr;
    XWalkContentUserData* data = reinterpret_cast<XWalkContentUserData*>(web_contents->GetUserData(
        kXWalkContentUserDataKey));
    return data ? data->content_ : nullptr;
  }

 private:
  XWalkContent* content_;
};

//// FIXME(wang16): Remove following methods after deprecated fields
//// are not supported any more.
//void PrintManifestDeprecationWarning(std::string field) {
//  LOG(WARNING) << "\"" << field << "\" is deprecated for Crosswalk. " << "Please follow "
//               << "https://www.crosswalk-project.org/#documentation/manifest.";
//}

//bool ManifestHasPath(const xwalk::application::Manifest& manifest, const std::string& path,
//                     const std::string& deprecated_path) {
//  if (manifest.HasPath(path))
//    return true;
//  if (manifest.HasPath(deprecated_path)) {
//    PrintManifestDeprecationWarning(deprecated_path);
//    return true;
//  }
//  return false;
//}
//
//bool ManifestGetString(const xwalk::application::Manifest& manifest, const std::string& path,
//                       const std::string& deprecated_path, std::string* out_value) {
//  if (manifest.GetString(path, out_value))
//    return true;
//  if (manifest.GetString(deprecated_path, out_value)) {
//    PrintManifestDeprecationWarning(deprecated_path);
//    return true;
//  }
//  return false;
//}

}  // namespace

static jlong JNI_XWalkContent_Init(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  std::unique_ptr<WebContents> web_contents(
      content::WebContents::Create(content::WebContents::CreateParams(XWalkRunner::GetInstance()->browser_context())));
  return reinterpret_cast<intptr_t>(new XWalkContent(std::move(web_contents)));
}

static ScopedJavaLocalRef<jstring> JNI_XWalkContent_GetChromeVersion(
    JNIEnv* env) {
  LOG(INFO) << "ChromeVersion=" << version_info::GetVersionNumber();
  return base::android::ConvertUTF8ToJavaString(env, version_info::GetVersionNumber());
}

// static
void JNI_XWalkContent_UpdateDefaultLocale(JNIEnv* env, const JavaParamRef<jstring>& locale,
                                          const JavaParamRef<jstring>& locale_list) {
  *g_locale() = ConvertJavaStringToUTF8(env, locale);
  *g_locale_list() = ConvertJavaStringToUTF8(env, locale_list);
}

// static
std::string XWalkContent::GetLocale() {
  return *g_locale();
}

// static
std::string XWalkContent::GetLocaleList() {
  return *g_locale_list();
}

// static
XWalkContent* XWalkContent::FromID(int render_process_id, int render_view_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  content::RenderViewHost* rvh = content::RenderViewHost::FromID(render_process_id, render_view_id);
  if (!rvh)
    return NULL;
  content::WebContents* web_contents = content::WebContents::FromRenderViewHost(rvh);
  if (!web_contents)
    return NULL;
  return FromWebContents(web_contents);
}

// static
XWalkContent* XWalkContent::FromWebContents(content::WebContents* web_contents) {
  return XWalkContentUserData::GetContents(web_contents);
}

XWalkContent::XWalkContent(std::unique_ptr<content::WebContents> web_contents)
    : web_contents_(std::move(web_contents)),
      _zone_id(0),
      _tab_id(0) {
  LOG(ERROR) << "iotto " << __func__ << " FIX AUTOFILL!";
//  xwalk_autofill_manager_.reset(new XWalkAutofillManager(web_contents_.get()));
  XWalkContentLifecycleNotifier::OnXWalkViewCreated();
}

void XWalkContent::SetXWalkAutofillClient(const base::android::JavaRef<jobject>& client) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_XWalkContent_setXWalkAutofillClient(env, obj, client);
}

void XWalkContent::SetSaveFormData(bool enabled) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  LOG(ERROR) << "iotto " << __func__ << " FIX AUTOFILL!";
//  xwalk_autofill_manager_->InitAutofillIfNecessary(enabled);
//  // We need to check for the existence, since autofill_manager_delegate
//  // may not be created when the setting is false.
//  XWalkAutofillClientAndroid *client = XWalkAutofillClientAndroid::FromWebContents(
//      web_contents_.get());
//  if (client != nullptr )
//    client->SetSaveFormData(enabled);
}

XWalkContent::~XWalkContent() {
  XWalkContentLifecycleNotifier::OnXWalkViewDestroyed();
}

void XWalkContent::SetJavaPeers(
    JNIEnv* env, const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jobject>& xwalk_content,
    const base::android::JavaParamRef<jobject>& web_contents_delegate,
    const base::android::JavaParamRef<jobject>& contents_client_bridge,
    const base::android::JavaParamRef<jobject>& io_thread_client,
    const base::android::JavaParamRef<jobject>& intercept_navigation_delegate) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  java_ref_ = JavaObjectWeakGlobalRef(env, xwalk_content);

  web_contents_delegate_.reset(new XWalkWebContentsDelegate(env, web_contents_delegate));
  contents_client_bridge_.reset(new XWalkContentsClientBridge(env, contents_client_bridge, web_contents_.get()));

  web_contents_->SetUserData(kXWalkContentUserDataKey, std::make_unique<XWalkContentUserData>(this));

  XWalkContentsIoThreadClient::RegisterPendingContents(web_contents_.get());

#ifdef TENTA_CHROMIUM_BUILD
  _zone_id = Java_XWalkContent_getZoneId(env, obj);
  _tab_id = Java_XWalkContent_getTabId(env, obj);

  TentaTabModel::NewTabForWebContents(web_contents_.get(), TentaZoneTabID(_zone_id, _tab_id));

  // Net Error handler on client side
  TentaNetErrorClient::CreateForWebContents(web_contents_.get());
  TentaNetErrorClient::FromWebContents(web_contents_.get())->SetListener(this);

  TentaTabHistory::CreateForWebContents(web_contents_.get());

#else
  // dummy call to avoid compiler warning
  Java_XWalkContent_getZoneId(env, obj);
  Java_XWalkContent_getTabId(env, obj);
#endif // TENTA_CHROMIUM_BUILD

//  // XWalk does not use disambiguation popup for multiple targets.
//  blink::mojom::RendererPreferences* prefs = web_contents_->GetMutableRendererPrefs();

  XWalkContentsClientBridge::Associate(web_contents_.get(),
                                           contents_client_bridge_.get());
  XWalkContentsIoThreadClient::Associate(web_contents_.get(), io_thread_client);

  for (content::RenderFrameHost* rfh : web_contents_->GetAllFrames()) {
    int render_process_id = rfh->GetProcess()->GetID();
    int render_frame_id = rfh->GetRoutingID();

    RuntimeResourceDispatcherHostDelegateAndroid::OnIoThreadClientReady(render_process_id, render_frame_id);
  }
  InterceptNavigationDelegate::Associate(
      web_contents_.get(), base::WrapUnique(new InterceptNavigationDelegate(env, intercept_navigation_delegate)));
  web_contents_->SetDelegate(web_contents_delegate_.get());

  render_view_host_ext_.reset(new XWalkRenderViewHostExt(web_contents_.get()));

  // TODO(iotto): Continue implementation
  GetJsJavaConfiguratorHost();
}

base::android::ScopedJavaLocalRef<jobject> XWalkContent::GetWebContents(JNIEnv* env, jobject obj) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(web_contents_);
  if (!web_contents_)
    return base::android::ScopedJavaLocalRef<jobject>();
  return web_contents_->GetJavaWebContents();
}

void XWalkContent::SetPendingWebContentsForPopup(std::unique_ptr<content::WebContents> pending) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (pending_contents_.get()) {
    // TODO(benm): Support holding multiple pop up window requests.
    LOG(WARNING) << "Blocking popup window creation as an outstanding " << "popup window is still pending.";
    base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, pending.release());
    return;
  }
  pending_contents_.reset(new XWalkContent(std::move(pending)));
  LOG(ERROR) << "iotto " << __func__ << " FIX";
//  // Set dip_scale for pending contents, which is necessary for the later
//  // SynchronousCompositor and InputHandler setup.
//  pending_contents_->SetDipScaleInternal(browser_view_renderer_.dip_scale());
}

jlong XWalkContent::ReleasePopupXWalkContent(JNIEnv* env, jobject obj) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return reinterpret_cast<intptr_t>(pending_contents_.release());
}

void XWalkContent::ClearCache(JNIEnv* env, jobject obj, jboolean include_disk_files) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  render_view_host_ext_->ClearCache();

  if (include_disk_files) {
    content::BrowsingDataRemover* remover = content::BrowserContext::GetBrowsingDataRemover(
        web_contents_->GetBrowserContext());
    remover->Remove(
        base::Time(),
        base::Time::Max(),
        content::BrowsingDataRemover::DATA_TYPE_CACHE,
        content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB | content::BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB);
  }

//  if (include_disk_files)
//    RemoveHttpDiskCache(web_contents_->GetRenderProcessHost(), std::string());
}

void XWalkContent::ClearCacheForSingleFile(JNIEnv* env, jobject obj,
                                           jstring url) {
  // TODO(iotto) : Use BrowsingDateRemover with FilterBuilder as in ClearCache
//
//  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
//  std::string key = base::android::ConvertJavaStringToUTF8(env, url);
//
//  if (!key.empty())
//    RemoveHttpDiskCache(web_contents_->GetRenderProcessHost(), key);
}

ScopedJavaLocalRef<jstring> XWalkContent::DevToolsAgentId(JNIEnv* env, jobject obj) {
  scoped_refptr<content::DevToolsAgentHost> agent_host(content::DevToolsAgentHost::GetOrCreateFor(web_contents_.get()));
  return base::android::ConvertUTF8ToJavaString(env, agent_host->GetId());
}

void XWalkContent::Destroy(JNIEnv* env, jobject obj) {
#if defined(TENTA_CHROMIUM_BUILD)
  ::tenta::fs::cache::MetaCacheBackend* backend = ::tenta::fs::cache::MetaCacheBackend::GetInstance();
  backend->FlushBuffer();
#endif
  delete this;
}

void XWalkContent::RequestNewHitTestDataAt(
    JNIEnv* env, const base::android::JavaParamRef<jobject>& obj, jfloat x,
    jfloat y, jfloat touch_major) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  gfx::PointF touch_center(x, y);
  gfx::SizeF touch_area(touch_major, touch_major);
  render_view_host_ext_->RequestNewHitTestDataAt(touch_center, touch_area);
}

void XWalkContent::UpdateLastHitTestData(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!render_view_host_ext_->HasNewHitTestData())
    return;

  const XWalkHitTestData& data = render_view_host_ext_->GetLastHitTestData();
  render_view_host_ext_->MarkHitTestDataRead();

  // Make sure to null the java object if data is empty/invalid
  ScopedJavaLocalRef<jstring> extra_data_for_type;
  if (data.extra_data_for_type.length())
    extra_data_for_type = ConvertUTF8ToJavaString(env, data.extra_data_for_type);

  ScopedJavaLocalRef<jstring> href;
  if (data.href.length())
    href = ConvertUTF16ToJavaString(env, data.href);

  ScopedJavaLocalRef<jstring> anchor_text;
  if (data.anchor_text.length())
    anchor_text = ConvertUTF16ToJavaString(env, data.anchor_text);

  ScopedJavaLocalRef<jstring> img_src;
  if (data.img_src.is_valid())
    img_src = ConvertUTF8ToJavaString(env, data.img_src.spec());
  Java_XWalkContent_updateHitTestData(env, obj, data.type, extra_data_for_type, href, anchor_text, img_src);
}

ScopedJavaLocalRef<jstring> XWalkContent::GetVersion(JNIEnv* env, jobject obj) {
  return base::android::ConvertUTF8ToJavaString(env, XWALK_VERSION);
}

void XWalkContent::SetJsOnlineProperty(JNIEnv* env, jobject obj, jboolean network_up) {
  render_view_host_ext_->SetJsOnlineProperty(network_up);
}

JsJavaConfiguratorHost* XWalkContent::GetJsJavaConfiguratorHost() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!js_java_configurator_host_.get()) {
    js_java_configurator_host_ =
        std::make_unique<JsJavaConfiguratorHost>(web_contents_.get());
  }
  return js_java_configurator_host_.get();
}

jboolean XWalkContent::SetManifest(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj,
                                   const base::android::JavaParamRef<jstring>& path,
                                   const base::android::JavaParamRef<jstring>& manifest_string) {
  LOG(ERROR) << "iotto " << __func__ << " FIX if in use";
//  std::string path_str = base::android::ConvertJavaStringToUTF8(env, path);
//  std::string json_input = base::android::ConvertJavaStringToUTF8(env, manifest_string);
//
//  std::unique_ptr<base::Value> manifest_value = base::JSONReader::Read(json_input);
//  if (!manifest_value || !manifest_value->IsType(base::Value::Type::DICTIONARY))
//    return false;
//
//  xwalk::application::Manifest manifest(
//      base::WrapUnique(static_cast<base::DictionaryValue*>(manifest_value.release())));
//
//  std::string url;
//  if (manifest.GetString(keys::kStartURLKey, &url)) {
//    std::string scheme = GURL(url).scheme();
//    if (scheme.empty())
//      url = path_str + url;
//  } else if (manifest.GetString(keys::kLaunchLocalPathKey, &url)) {
//    PrintManifestDeprecationWarning(keys::kLaunchLocalPathKey);
//    // According to original proposal for "app:launch:local_path", the "http"
//    // and "https" schemes are supported. So |url| should do nothing when it
//    // already has "http" or "https" scheme.
//    std::string scheme = GURL(url).scheme();
//    if (scheme != url::kHttpScheme && scheme != url::kHttpsScheme)
//      url = path_str + url;
//  } else if (manifest.GetString(keys::kLaunchWebURLKey, &url)) {
//    PrintManifestDeprecationWarning(keys::kLaunchWebURLKey);
//  } else {
//    NOTIMPLEMENTED()
//    ;
//  }
//
//  std::string match_patterns;
//  const base::ListValue* xwalk_hosts = NULL;
//  if (manifest.GetList(xwalk::application_manifest_keys::kXWalkHostsKey, &xwalk_hosts)) {
//    base::JSONWriter::Write(*xwalk_hosts, &match_patterns);
//  }
//  render_view_host_ext_->SetOriginAccessWhitelist(url, match_patterns);
//
//  std::string csp;
//  ManifestGetString(manifest, keys::kCSPKey, keys::kDeprecatedCSPKey, &csp);
//  XWalkBrowserContext* browser_context = XWalkRunner::GetInstance()->browser_context();
//  CHECK(browser_context);
//  browser_context->SetCSPString(csp);
//
//  ScopedJavaLocalRef<jstring> url_buffer = base::android::ConvertUTF8ToJavaString(env, url);
//
//  if (manifest.HasPath(kDisplay)) {
//    std::string display_string;
//    if (manifest.GetString(kDisplay, &display_string)) {
//      // TODO(David): update the handling process of the display strings
//      // including fullscreen etc.
//      bool display_as_fullscreen = base::LowerCaseEqualsASCII(display_string, "fullscreen");
//      Java_XWalkContent_onGetFullscreenFlagFromManifest(env, obj, display_as_fullscreen ? JNI_TRUE : JNI_FALSE);
//    }
//  }
//
//  // Check whether need to display launch screen. (Read from manifest.json)
//  if (ManifestHasPath(manifest, keys::kXWalkLaunchScreen, keys::kLaunchScreen)) {
//    std::string ready_when;
//    // Get the value of 'ready_when' from manifest.json
//    ManifestGetString(manifest, keys::kXWalkLaunchScreenReadyWhen, keys::kLaunchScreenReadyWhen, &ready_when);
//    ScopedJavaLocalRef<jstring> ready_when_buffer = base::android::ConvertUTF8ToJavaString(env, ready_when);
//
//    // Get the value of 'image_border'
//    // 1. When 'launch_screen.[orientation]' was defined, but no 'image_border'
//    //    The value of 'image_border' will be set as 'empty'.
//    // 2. Otherwise, there is no 'launch_screen.[orientation]' defined,
//    //    The value of 'image_border' will be empty.
//    const char empty[] = "empty";
//    std::string image_border_default;
//    ManifestGetString(manifest, keys::kXWalkLaunchScreenImageBorderDefault, keys::kLaunchScreenImageBorderDefault,
//                      &image_border_default);
//    if (image_border_default.empty() && ManifestHasPath(manifest, keys::kXWalkLaunchScreenDefault,
//                                                        keys::kLaunchScreenDefault)) {
//      image_border_default = empty;
//    }
//
//    std::string image_border_landscape;
//    ManifestGetString(manifest, keys::kXWalkLaunchScreenImageBorderLandscape, keys::kLaunchScreenImageBorderLandscape,
//                      &image_border_landscape);
//    if (image_border_landscape.empty() && ManifestHasPath(manifest, keys::kXWalkLaunchScreenLandscape,
//                                                          keys::kLaunchScreenLandscape)) {
//      image_border_landscape = empty;
//    }
//
//    std::string image_border_portrait;
//    ManifestGetString(manifest, keys::kXWalkLaunchScreenImageBorderPortrait, keys::kLaunchScreenImageBorderPortrait,
//                      &image_border_portrait);
//    if (image_border_portrait.empty() && ManifestHasPath(manifest, keys::kXWalkLaunchScreenPortrait,
//                                                         keys::kLaunchScreenPortrait)) {
//      image_border_portrait = empty;
//    }
//
//    std::string image_border = image_border_default + ';' + image_border_landscape + ';' + image_border_portrait;
//    ScopedJavaLocalRef<jstring> image_border_buffer = base::android::ConvertUTF8ToJavaString(env, image_border);
//
//    Java_XWalkContent_onGetUrlAndLaunchScreenFromManifest(
//                                                          env,
//                                                          obj, url_buffer,
//                                                          ready_when_buffer,
//                                                          image_border_buffer);
//  } else {
//    // No need to display launch screen, load the url directly.
//    Java_XWalkContent_onGetUrlFromManifest(env, obj, url_buffer);
//  }
//  std::string view_background_color;
//  ManifestGetString(manifest, keys::kXWalkViewBackgroundColor, keys::kViewBackgroundColor, &view_background_color);
//
//  if (view_background_color.empty())
//    return true;
//  unsigned int view_background_color_int = 0;
//  if (!base::HexStringToUInt(view_background_color.substr(1), &view_background_color_int)) {
//    LOG(ERROR) << "Background color format error! Valid background color"
//               "should be(Alpha Red Green Blue): #ff01abcd";
//    return false;
//  }
//  Java_XWalkContent_setBackgroundColor(env, obj, view_background_color_int);
  return true;
}

jint XWalkContent::GetRoutingID(JNIEnv* env, jobject obj) {
  DCHECK(web_contents_.get());
  return web_contents_->GetRenderViewHost()->GetRoutingID();
}

base::android::ScopedJavaLocalRef<jbyteArray> XWalkContent::GetState(JNIEnv* env, jobject obj) {
  if (!web_contents_->GetController().GetEntryCount())
    return ScopedJavaLocalRef<jbyteArray>();

  base::Pickle pickle;
  WriteToPickle(*web_contents_, &pickle);
  return base::android::ToJavaByteArray(env, reinterpret_cast<const uint8_t*>(pickle.data()), pickle.size());
}

jboolean XWalkContent::SetState(JNIEnv* env, const JavaParamRef<jobject>& obj, const JavaParamRef<jbyteArray>& state) {
  std::vector<uint8_t> state_vector;
  base::android::JavaByteArrayToByteVector(env, state, &state_vector);

  base::Pickle pickle(reinterpret_cast<const char*>(&state_vector[0]), state_vector.size());
  base::PickleIterator iterator(pickle);

  return RestoreFromPickle(&iterator, web_contents_.get());
}


/************** History in MetaFs *********************/
/**
 *
 */
#ifdef TENTA_CHROMIUM_BUILD
int XWalkContent::OpenHistoryFile(JNIEnv* env, const JavaParamRef<jstring>& id, const JavaParamRef<jstring>& key,
                                  scoped_refptr<::tenta::fs::MetaFile>& fileOut,
                                  scoped_refptr<::tenta::fs::MetaDb>& dbOut, int mode) {
  using namespace metafs;
  using namespace ::base::android;

  MetaFsManager * mng = MetaFsManager::GetInstance();
  if (mng == nullptr) {
#if TENTA_LOG_ENABLE == 1
    LOG(ERROR) << "MetaFsManager::GetInstance() NULL";
#endif
    return ERR_NULL_POINTER;
  }

  std::string keyStr;

  ConvertJavaStringToUTF8(env, key, &keyStr);

  int result;

  result = mng->OpenDb(cHistoryDb, keyStr, cHistoryBlockSize, dbOut);
  if (result != FS_OK) {
#if TENTA_LOG_ENABLE == 1
    LOG(ERROR) << "OpenDb failed";
#endif
    return result;
  }

  if (dbOut.get() == nullptr) {
#if TENTA_LOG_ENABLE == 1
    LOG(ERROR) << "MetaDb NULL";
#endif
    return ERR_INVALID_POINTER;
  }

  std::string idStr;
  ConvertJavaStringToUTF8(env, id, &idStr);

  scoped_refptr<MetaFile> weak_file;

  int status;
  status = dbOut->OpenFile(idStr, "", weak_file, mode);

  if (status != FS_OK) {
#if TENTA_LOG_ENABLE == 1
    LOG(ERROR) << "OpenFile '" << idStr << "' failed " << status;
#endif
    return status;
  }

  fileOut = weak_file;
  if (fileOut.get() == nullptr) {
#if TENTA_LOG_ENABLE == 1
    LOG(ERROR) << "File pointer NULL";
#endif
    return ERR_INVALID_POINTER;
  }

  return FS_OK;
}
#endif // TENTA_CHROMIUM_BUILD

/**
 *
 */
jint XWalkContent::SaveOldHistory(JNIEnv* env, const JavaParamRef<jobject>& jcaller,
                                  const JavaParamRef<jbyteArray>& state, const JavaParamRef<jstring>& id,
                                  const JavaParamRef<jstring>& key) {
#ifndef TENTA_CHROMIUM_BUILD
  LOG(WARNING) << __func__ << " Not implemented!";
  return -1;
#else // TENTA_CHROMIUM_BUILD

  using namespace metafs;

  scoped_refptr<MetaDb> db;
  scoped_refptr<MetaFile> file;
  AutoCloseMetaDb mdbClose(db);
  AutoCloseMetaFile mvfClose(file);

  int status;
  status = OpenHistoryFile(env, id, key, file, db, MetaDb::IO_CREATE_IF_NOT_EXISTS | MetaDb::IO_TRUNCATE_IF_EXISTS);

  if (status != FS_OK) {
    return status;
  }

  std::unique_ptr<JavaByteArray> data(new JavaByteArray());
  if (data.get() == nullptr) {
#if TENTA_LOG_ENABLE == 1
    LOG(ERROR) << "SaveOldHistory: JavaByteArray not created, out of memory";
#endif
    return ERR_OUT_MEMORY;
  }

  status = data->init(env, state);
  if (status != FS_OK) {
#if TENTA_LOG_ENABLE == 1
    LOG(ERROR) << "SaveOldHistory: JavaByteArray init failed " << status;
#endif
    return status;
  }

  int iolen = data->len();
  status = file->Append(data->data(), iolen);
  if (status != FS_OK) {
    return status;
  }

#if TENTA_LOG_ENABLE == 1
  LOG(INFO) << __func__ << "_OK length=" << iolen;
#endif
  return iolen;
#endif // TENTA_CHROMIUM_BUILD
}

/**
 *
 */
jint XWalkContent::SaveHistory(JNIEnv* env, const JavaParamRef<jobject>& obj, const JavaParamRef<jstring>& id,
                               const JavaParamRef<jstring>& key) {
#ifndef TENTA_CHROMIUM_BUILD
  LOG(WARNING) << __func__ << " Not implemented!";
  return -1;
#else // TENTA_CHROMIUM_BUILD

  using namespace metafs;
  using namespace ::base::android;

  std::unique_ptr<base::Pickle> pickle(new base::Pickle());

  if (pickle.get() == nullptr) {
#if TENTA_LOG_ENABLE == 1
    LOG(ERROR) << "SaveHistory: Pickle not created, out of memory";
#endif
    return ERR_OUT_MEMORY;
  }

  WriteToPickle(*web_contents_, pickle.get());

  scoped_refptr<MetaDb> db;
  scoped_refptr<MetaFile> file;
  AutoCloseMetaDb mdbClose(db);
  AutoCloseMetaFile mvfClose(file);

  int status;
  status = OpenHistoryFile(env, id, key, file, db, MetaDb::IO_CREATE_IF_NOT_EXISTS | MetaDb::IO_TRUNCATE_IF_EXISTS);

  if (status != FS_OK) {
    return status;
  }

  int iolen = pickle->size();
  status = file->Append(reinterpret_cast<const char*>(pickle->data()), iolen);
  if (status != FS_OK) {
    return status;
  }

#if TENTA_LOG_ENABLE == 1
  LOG(ERROR) << __func__ << "_OK length=" << iolen;
#endif
  return iolen;
#endif // TENTA_CHROMIUM_BUILD
}

/**
 *
 */
jint XWalkContent::RestoreHistory(JNIEnv* env, const JavaParamRef<jobject>& obj, const JavaParamRef<jstring>& id,
                                  const JavaParamRef<jstring>& key) {
#ifndef TENTA_CHROMIUM_BUILD
  LOG(WARNING) << __func__ << " Not implemented!";
  return -1;
#else // TENTA_CHROMIUM_BUILD

  using namespace metafs;
  using namespace ::base::android;

  scoped_refptr<MetaDb> db;
  scoped_refptr<MetaFile> file;
  AutoCloseMetaDb mdbClose(db);
  AutoCloseMetaFile mvfClose(file);

  int status;
  status = OpenHistoryFile(env, id, key, file, db, MetaDb::IO_CREATE_IF_NOT_EXISTS | MetaDb::IO_OPEN_EXISTING);

  if (status != FS_OK) {
    return status;
  }

  int length = file->length();

  if (length < 0) {
#if TENTA_LOG_ENABLE == 1
    LOG(ERROR) << "RestoreHistory get file length " << length;
#endif
    return ERR_INVALID_FILE_DATA;
  }

  if (length == 0) {
#if TENTA_LOG_ENABLE == 1
    LOG(INFO) << "RestoreHistory empty " << length;
#endif
    return FS_OK;  // zero data
    // Safe to return since this webView was just created
    // so it has no history to be cleared
  }

  std::vector<char> state_vector;
  state_vector.resize(length);

  status = file->Read(0, &state_vector[0], length, nullptr);
  if (status != FS_OK) {
#if TENTA_LOG_ENABLE == 1
    LOG(ERROR) << "RestoreHistory read error " << status;
#endif
    return status;
  }

  base::Pickle pickle(&state_vector[0], length);
  state_vector.clear();

  base::PickleIterator iterator(pickle);
  if (!RestoreFromPickle(&iterator, web_contents_.get())) {
#if TENTA_LOG_ENABLE == 1
    LOG(ERROR) << "Restore pickle error " << pickle.size() << " payLoad: " << pickle.payload_size();
#endif
    return ERR_XWALK_INTERNAL;
  }

#if TENTA_LOG_ENABLE == 1
  LOG(ERROR) << __func__ << "_OK length=" << pickle.payload_size();
#endif

  return pickle.payload_size();
#endif // TENTA_CHROMIUM_BUILD
}

/**
 *
 */
jint XWalkContent::NukeHistory(JNIEnv* env, const JavaParamRef<jobject>& obj, const JavaParamRef<jstring>& id,
                               const JavaParamRef<jstring>& key) {
#ifndef TENTA_CHROMIUM_BUILD
  LOG(WARNING) << __func__ << " Not implemented!";
  return -1;
#else // TENTA_CHROMIUM_BUILD

  using namespace metafs;

  scoped_refptr<MetaDb> db;
  scoped_refptr<MetaFile> file;
  AutoCloseMetaDb mdbClose(db);
  AutoCloseMetaFile mvfClose(file);

  int status;
  status = OpenHistoryFile(env, id, key, file, db, MetaDb::IO_OPEN_EXISTING);

  if (status != FS_OK) {
    return status;
  }

  status = file->Delete();

  if (status != FS_OK) {
    return status;
  }

#if TENTA_LOG_ENABLE == 1
  LOG(ERROR) << __func__ << "_OK length=" << 0;
#endif

  return FS_OK;
#endif // TENTA_CHROMIUM_BUILD
}

/************ End MetaFS ****************/
namespace {

void ShowGeolocationPromptHelperTask(const JavaObjectWeakGlobalRef& java_ref, const GURL& origin) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_ref = java_ref.get(env);
  if (j_ref.obj()) {
    ScopedJavaLocalRef<jstring> j_origin(
                                         ConvertUTF8ToJavaString(env, origin.spec()));
    Java_XWalkContent_onGeolocationPermissionsShowPrompt(env, j_ref,
                                                         j_origin);
  }
}

void ShowGeolocationPromptHelper(const JavaObjectWeakGlobalRef& java_ref, const GURL& origin) {
  JNIEnv* env = AttachCurrentThread();
  if (java_ref.get(env).obj()) {
    base::PostTaskWithTraits(FROM_HERE, {content::BrowserThread::UI},
    base::BindOnce(&ShowGeolocationPromptHelperTask,
        java_ref,
        origin));
  }
}
}
// anonymous namespace

void XWalkContent::ShowGeolocationPrompt(const GURL& requesting_frame,
                                         base::OnceCallback<void(bool)> callback) {  // NOLINT
      GURL origin = requesting_frame.GetOrigin();
      bool show_prompt = pending_geolocation_prompts_.empty();
      pending_geolocation_prompts_.push_back(OriginCallback(origin, std::move(callback)));
      if (show_prompt) {
        ShowGeolocationPromptHelper(java_ref_, origin);
      }
    }

// Called by Java.
void XWalkContent::InvokeGeolocationCallback(JNIEnv* env, jobject obj, jboolean value, jstring origin) {
  GURL callback_origin(base::android::ConvertJavaStringToUTF16(env, origin));
  if (callback_origin.GetOrigin() == pending_geolocation_prompts_.front().first) {
    std::move(pending_geolocation_prompts_.front().second).Run(value);
    pending_geolocation_prompts_.pop_front();
    if (!pending_geolocation_prompts_.empty()) {
      ShowGeolocationPromptHelper(java_ref_, pending_geolocation_prompts_.front().first);
    }
  }
}

void XWalkContent::HideGeolocationPrompt(const GURL& origin) {
  bool removed_current_outstanding_callback = false;
  std::list<OriginCallback>::iterator it = pending_geolocation_prompts_.begin();
  while (it != pending_geolocation_prompts_.end()) {
    if ((*it).first == origin.GetOrigin()) {
      if (it == pending_geolocation_prompts_.begin()) {
        removed_current_outstanding_callback = true;
      }
      it = pending_geolocation_prompts_.erase(it);
    } else {
      ++it;
    }
  }

  if (removed_current_outstanding_callback) {
    JNIEnv* env = AttachCurrentThread();
    ScopedJavaLocalRef<jobject> j_ref = java_ref_.get(env);
    if (j_ref.obj()) {
      Java_XWalkContent_onGeolocationPermissionsHidePrompt(env, j_ref);
    }
    if (!pending_geolocation_prompts_.empty()) {
      ShowGeolocationPromptHelper(java_ref_, pending_geolocation_prompts_.front().first);
    }
  }
}

// Called by Java.
void XWalkContent::SetBackgroundColor(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj, jint color) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  render_view_host_ext_->SetBackgroundColor(color);
}

void XWalkContent::SetOriginAccessWhitelist(JNIEnv* env, jobject obj, jstring url, jstring match_patterns) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  render_view_host_ext_->SetOriginAccessWhitelist(base::android::ConvertJavaStringToUTF8(env, url),
                                                  base::android::ConvertJavaStringToUTF8(env, match_patterns));
}

base::android::ScopedJavaLocalRef<jbyteArray> XWalkContent::GetCertificate(JNIEnv* env,
                                                                           const JavaParamRef<jobject>& obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  content::NavigationEntry* entry = web_contents_->GetController().GetLastCommittedEntry();
  if (!entry || !entry->GetSSL().certificate) {
    return ScopedJavaLocalRef<jbyteArray>();
  }

  // Convert the certificate and return it
  base::StringPiece der_string = net::x509_util::CryptoBufferAsStringPiece(
      entry->GetSSL().certificate->cert_buffer());
  return base::android::ToJavaByteArray(env, reinterpret_cast<const uint8_t*>(der_string.data()), der_string.length());
}

/**
 *
 */
base::android::ScopedJavaLocalRef<jobjectArray> XWalkContent::GetCertificateChain(JNIEnv* env,
                                                                                  const JavaParamRef<jobject>& obj) {

  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  content::NavigationEntry* entry = web_contents_->GetController().GetLastCommittedEntry();
  if (!entry || !entry->GetSSL().certificate) {
    return ScopedJavaLocalRef<jobjectArray>();
  }

  // Get the certificate

  scoped_refptr<net::X509Certificate> cert = entry->GetSSL().certificate;

  std::vector<std::string> cert_chain;
  // Convert the certificate and return it
  base::StringPiece der_string = net::x509_util::CryptoBufferAsStringPiece(
      entry->GetSSL().certificate->cert_buffer());

  cert_chain.push_back(der_string.as_string());  // store main cert

  // iterate over the list of intermediates
//  const net::X509Certificate::OSCertHandles& intermediates = cert->intermediate_buffers();
  const std::vector<bssl::UniquePtr<CRYPTO_BUFFER>>& intermediates = cert->intermediate_buffers();

  for (size_t i = 0; i < intermediates.size(); ++i) {
    der_string = net::x509_util::CryptoBufferAsStringPiece(
        intermediates[i].get());

    cert_chain.push_back(der_string.as_string());  // store intermediates
  }
  return base::android::ToJavaArrayOfByteArray(env, cert_chain);
}

FindHelper* XWalkContent::GetFindHelper() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!find_helper_.get()) {
    find_helper_.reset(new FindHelper(web_contents_.get()));
    find_helper_->SetListener(this);
  }
  return find_helper_.get();
}

void XWalkContent::FindAllAsync(JNIEnv* env, const JavaParamRef<jobject>& obj,
                                const JavaParamRef<jstring>& search_string) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GetFindHelper()->FindAllAsync(ConvertJavaStringToUTF16(env, search_string));
}

void XWalkContent::FindNext(JNIEnv* env, const JavaParamRef<jobject>& obj, jboolean forward) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GetFindHelper()->FindNext(forward);
}

void XWalkContent::ClearMatches(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GetFindHelper()->ClearMatches();
}

void XWalkContent::CaptureBitmapWithParams(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj,
                                           float scale, const base::android::JavaParamRef<jobject>& jcallback) {
  LOG(ERROR) << "iotto " << __func__ << " IMPLEMENT!";
  content::RenderWidgetHostView* rwhv = web_contents_->GetRenderWidgetHostView();
  if ( rwhv) {
    base::OnceCallback<void(const SkBitmap&)> result_callback = base::BindOnce(
        &XWalkContent::OnDidCaptureBitmap, _weak_ptr_factory.GetWeakPtr(),
        base::android::ScopedJavaGlobalRef<jobject>(env, obj),
        base::android::ScopedJavaGlobalRef<jobject>(env, jcallback), scale);
    rwhv->CopyFromSurface(gfx::Rect(), gfx::Size(0, 0), std::move(result_callback));
  } else {
    base::android::RunObjectCallbackAndroid(jcallback, ScopedJavaLocalRef<jobject>());
  }
}

void XWalkContent::OnDidCaptureBitmap(const base::android::JavaRef<jobject>& obj, const base::android::JavaRef<jobject>& callback,
                          float scale, const SkBitmap& bitmap) {
  TENTA_LOG(WARNING) << __func__ << " TODO(iotto): scale parameter ignored! Fix it!";

  if (!bitmap.drawsNothing()) {
    ScopedJavaLocalRef<jobject> jbitmap = gfx::ConvertToJavaBitmap(&bitmap);
    base::android::RunObjectCallbackAndroid(callback, jbitmap);
    return;
  }
  // If readback failed, call empty callback
  base::android::RunObjectCallbackAndroid(callback, ScopedJavaLocalRef<jobject>());
}

void XWalkContent::OnFindResultReceived(int active_ordinal, int match_count, bool finished) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  Java_XWalkContent_onFindResultReceived(env, obj, active_ordinal, match_count, finished);
}

/**
 *
 */
void XWalkContent::OnOpenDnsSettings(const GURL& failedUrl) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  ScopedJavaLocalRef<jstring> j_failed_url(ConvertUTF8ToJavaString(env, failedUrl.spec()));

  Java_XWalkContent_onOpenDnsSettings(env, obj, j_failed_url);
}

}  // namespace xwalk
