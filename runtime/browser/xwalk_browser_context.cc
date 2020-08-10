// Copyright (c) 2013 Intel Corporation. All rights reserved.
// Copyright (c) 2014 Samsung Electronics Co., Ltd All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "xwalk/runtime/browser/xwalk_browser_context.h"

#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/memory/scoped_refptr.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/user_prefs/user_prefs.h"
#include "components/variations/net/variations_http_headers.h"
#include "components/visitedlink/browser/visitedlink_master.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/cors_exempt_headers.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "xwalk/application/browser/application.h"
#include "xwalk/application/browser/application_protocols.h"
#include "xwalk/application/browser/application_service.h"
#include "xwalk/application/browser/application_system.h"
#include "xwalk/application/common/constants.h"
#include "xwalk/runtime/browser/runtime_download_manager_delegate.h"
#include "xwalk/runtime/browser/runtime_url_request_context_getter.h"
#include "xwalk/runtime/browser/xwalk_content_settings.h"
#include "xwalk/runtime/browser/xwalk_permission_manager.h"
#include "xwalk/runtime/browser/xwalk_pref_store.h"
#include "xwalk/runtime/browser/xwalk_runner.h"
#include "xwalk/runtime/browser/xwalk_special_storage_policy.h"
#include "xwalk/runtime/common/xwalk_paths.h"
#include "xwalk/runtime/common/xwalk_switches.h"

#if defined(OS_ANDROID)
#include "base/strings/string_split.h"
#elif defined(OS_WIN)
#include "base/base_paths_win.h"
#elif defined(OS_LINUX)
#include "base/nix/xdg_util.h"
#elif defined(OS_MACOSX)
#include "base/base_paths_mac.h"
#endif

using content::BrowserThread;
using content::DownloadManager;

namespace xwalk {

class XWalkBrowserContext::RuntimeResourceContext :
    public content::ResourceContext {
 public:
  RuntimeResourceContext()
      : getter_(NULL) {
  }
  ~RuntimeResourceContext() override {
  }

  // TODO(iotto): Removed in base ... we should too
  net::HostResolver* GetHostResolver() {
    CHECK(getter_);
    return getter_->host_resolver();
  }
  net::URLRequestContext* GetRequestContext() {
    CHECK(getter_);
    return getter_->GetURLRequestContext();
  }

  void set_url_request_context_getter(RuntimeURLRequestContextGetter* getter) {
    getter_ = getter;
  }

 private:
  RuntimeURLRequestContextGetter* getter_;

  DISALLOW_COPY_AND_ASSIGN(RuntimeResourceContext)
  ;
};

XWalkBrowserContext* g_browser_context = nullptr;

void HandleReadError(PersistentPrefStore::PrefReadError error) {
  if (error != PersistentPrefStore::PREF_READ_ERROR_NONE) {
    LOG(ERROR) << "Failed to read preference, error num: " << error;
  }
}

XWalkBrowserContext::XWalkBrowserContext()
    : resource_context_(new RuntimeResourceContext),
      save_form_data_(true) {
  InitWhileIOAllowed();
  InitFormDatabaseService();
  InitVisitedLinkMaster();
  CHECK(!g_browser_context);
  g_browser_context = this;
}

XWalkBrowserContext::~XWalkBrowserContext() {
#if !defined(OS_ANDROID)
  XWalkContentSettings::GetInstance()->Shutdown();
#endif
  if (resource_context_.get()) {
    BrowserThread::DeleteSoon(BrowserThread::IO,
    FROM_HERE, resource_context_.release());
  }
  DCHECK_EQ(this, g_browser_context);
  g_browser_context = nullptr;
  NotifyWillBeDestroyed(this);
  BrowserContextDependencyManager::GetInstance()->DestroyBrowserContextServices(
  this);
  ShutdownStoragePartitions();
}

// static
XWalkBrowserContext* XWalkBrowserContext::GetDefault() {
  // TODO(joth): rather than store in a global here, lookup this instance
  // from the Java-side peer.
  return g_browser_context;
}

// static
XWalkBrowserContext* XWalkBrowserContext::FromWebContents(
                                                          content::WebContents* web_contents) {
  // This is safe; this is the only implementation of the browser context.
  return static_cast<XWalkBrowserContext*>(
  web_contents->GetBrowserContext());
}

void XWalkBrowserContext::InitWhileIOAllowed() {
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  base::FilePath path;
  if (cmd_line->HasSwitch(switches::kXWalkDataPath)) {
    path = cmd_line->GetSwitchValuePath(switches::kXWalkDataPath);
    base::PathService::OverrideAndCreateIfNeeded(
                                           DIR_DATA_PATH,
                                           path, false, true);
  } else {
    base::FilePath::StringType xwalk_suffix;
    xwalk_suffix = FILE_PATH_LITERAL("xwalk");
#if defined(OS_WIN)
    CHECK(PathService::Get(base::DIR_LOCAL_APP_DATA, &path));
    path = path.Append(xwalk_suffix);
#elif defined(OS_LINUX)
    std::unique_ptr<base::Environment> env(base::Environment::Create());
    base::FilePath config_dir(
        base::nix::GetXDGDirectory(env.get(),
            base::nix::kXdgConfigHomeEnvVar,
            base::nix::kDotConfigDir));
    path = config_dir.Append(xwalk_suffix);
#elif defined(OS_MACOSX)
    CHECK(PathService::Get(base::DIR_APP_DATA, &path));
    path = path.Append(xwalk_suffix);
#elif defined(OS_ANDROID)
    CHECK(base::PathService::Get(base::DIR_ANDROID_APP_DATA, &path));
    path = path.Append(xwalk_suffix);
#else
    NOTIMPLEMENTED();
#endif
  }

  BrowserContext::Initialize(this, path);
#if !defined(OS_ANDROID)
  XWalkContentSettings::GetInstance()->Init();
#endif
}

#if !defined(OS_ANDROID)
std::unique_ptr<content::ZoomLevelDelegate> XWalkBrowserContext::CreateZoomLevelDelegate(
    const base::FilePath& partition_path) {
  return nullptr;
}
#endif

base::FilePath XWalkBrowserContext::GetPath() {
  base::FilePath result;
#if defined(OS_ANDROID)
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (cmd_line->HasSwitch(switches::kUserDataDir))
    result = cmd_line->GetSwitchValuePath(switches::kUserDataDir);
  if (result.empty())
    CHECK(base::PathService::Get(base::DIR_ANDROID_APP_DATA, &result));
  if (cmd_line->HasSwitch(switches::kXWalkProfileName))
    result = result.Append(
                           cmd_line->GetSwitchValuePath(switches::kXWalkProfileName));
#else
  CHECK(PathService::Get(DIR_DATA_PATH, &result));
#endif
  return result;
}

bool XWalkBrowserContext::IsOffTheRecord() {
  // We don't consider off the record scenario.
  return false;
}

content::DownloadManagerDelegate*
XWalkBrowserContext::GetDownloadManagerDelegate() {
  content::DownloadManager* manager = BrowserContext::GetDownloadManager(this);

  if (!download_manager_delegate_.get()) {
    download_manager_delegate_ = new RuntimeDownloadManagerDelegate();
    download_manager_delegate_->SetDownloadManager(manager);
  }

  return download_manager_delegate_.get();
}

content::ResourceContext* XWalkBrowserContext::GetResourceContext() {
  return resource_context_.get();
}

content::BrowserPluginGuestManager*
XWalkBrowserContext::GetGuestManager() {
  return NULL;
}

storage::SpecialStoragePolicy* XWalkBrowserContext::GetSpecialStoragePolicy() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
                                                        switches::kUnlimitedStorage)) {
    if (!special_storage_policy_.get())
      special_storage_policy_ = new XWalkSpecialStoragePolicy();
    return special_storage_policy_.get();
  }
  return NULL;
}

content::PushMessagingService* XWalkBrowserContext::GetPushMessagingService() {
  return NULL;
}

content::SSLHostStateDelegate* XWalkBrowserContext::GetSSLHostStateDelegate() {
  if (!ssl_host_state_delegate_.get()) {
    ssl_host_state_delegate_.reset(new XWalkSSLHostStateDelegate());
  }
  return ssl_host_state_delegate_.get();
}

content::PermissionControllerDelegate* XWalkBrowserContext::GetPermissionControllerDelegate() {
  if (!permission_manager_.get())
    permission_manager_.reset(new XWalkPermissionManager(/*application_service_*/));
  return permission_manager_.get();
}

content::ClientHintsControllerDelegate*
XWalkBrowserContext::GetClientHintsControllerDelegate() {
  return nullptr;
}

content::BackgroundFetchDelegate* XWalkBrowserContext::GetBackgroundFetchDelegate() {
  LOG(WARNING) << __func__ << " not_implemented";
  return nullptr;
}

content::BackgroundSyncController*
XWalkBrowserContext::GetBackgroundSyncController() {
  return nullptr;
}

content::BrowsingDataRemoverDelegate* XWalkBrowserContext::GetBrowsingDataRemoverDelegate() {
  LOG(WARNING) << __func__ << " not_implemented";
  return nullptr;
}

RuntimeURLRequestContextGetter*
XWalkBrowserContext::GetURLRequestContextGetterById(
                                                    const std::string& pkg_id) {
  for (PartitionPathContextGetterMap::iterator it = context_getters_.begin();
      it != context_getters_.end(); ++it) {
#if defined(OS_WIN)
    if (it->first.find(base::UTF8ToWide(pkg_id)))
#else
    if (it->first.find(pkg_id))
#endif
      return it->second.get();
  }
  return 0;
}

net::URLRequestContextGetter* XWalkBrowserContext::CreateRequestContext(
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) {
  if (url_request_getter_)
    return url_request_getter_.get();

//  protocol_handlers->insert(
//      std::pair<std::string,
//          std::unique_ptr<net::URLRequestJobFactory::ProtocolHandler> >(
//          application::kApplicationScheme,
//          application::CreateApplicationProtocolHandler(
//                                                        application_service_)));

  url_request_getter_ = new RuntimeURLRequestContextGetter(
      false, /* ignore_certificate_error = false */
      GetPath(),
      base::CreateSingleThreadTaskRunnerWithTraits({content::BrowserThread::IO}),
      base::CreateSingleThreadTaskRunnerWithTraits(
                {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
                 base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN}),
//      content::BrowserThread::GetTaskRunnerForThread(BrowserThread::FILE),
      protocol_handlers,
      std::move(request_interceptors));
  resource_context_->set_url_request_context_getter(url_request_getter_.get());
  return url_request_getter_.get();
}

net::URLRequestContextGetter* XWalkBrowserContext::CreateMediaRequestContext() {
  return url_request_getter_.get();
}

XWalkFormDatabaseService* XWalkBrowserContext::GetFormDatabaseService() {
  return form_database_service_.get();
}

// Create user pref service for autofill functionality.
void XWalkBrowserContext::CreateUserPrefServiceIfNecessary() {
  if (user_pref_service_)
    return;

  LOG(INFO) << "iotto " << __func__;
  PrefRegistrySimple* pref_registry = new PrefRegistrySimple();
  pref_registry->RegisterStringPref("intl.accept_languages", "");

  // We only use the autocomplete feature of Autofill, which is controlled via
  // the manager_delegate. We don't use the rest of Autofill, which is why it is
  // hardcoded as disabled here.
  // TODO(crbug.com/873740): The following also disables autocomplete.
  // Investigate what the intended behavior is.
  pref_registry->RegisterBooleanPref(autofill::prefs::kAutofillProfileEnabled,
                                false);
  pref_registry->RegisterBooleanPref(autofill::prefs::kAutofillCreditCardEnabled,
                                false);

  PrefServiceFactory pref_service_factory;
  pref_service_factory.set_user_prefs(base::MakeRefCounted<XWalkPrefStore>());
  pref_service_factory.set_read_error_callback(base::Bind(&HandleReadError));
  user_pref_service_ = pref_service_factory.Create(pref_registry);

  user_prefs::UserPrefs::Set(this, user_pref_service_.get());
}

void XWalkBrowserContext::UpdateAcceptLanguages(
                                                const std::string& accept_languages) {
  if (url_request_getter_)
    url_request_getter_->UpdateAcceptLanguages(accept_languages);
}

void XWalkBrowserContext::InitFormDatabaseService() {
  base::FilePath user_data_dir;
#if defined(OS_ANDROID)
  CHECK(base::PathService::Get(base::DIR_ANDROID_APP_DATA, &user_data_dir));
#elif defined(OS_WIN)
  CHECK(base::PathService::Get(base::DIR_APP_DATA, &user_data_dir));
#endif
  form_database_service_.reset(new XWalkFormDatabaseService(user_data_dir));
}

#if defined(OS_ANDROID)
void XWalkBrowserContext::SetCSPString(const std::string& csp) {
  // Check format of csp string.
  std::vector<std::string> policies = base::SplitString(
                                                        csp,
                                                        ";", base::TRIM_WHITESPACE,
                                                        base::SPLIT_WANT_ALL);
  for (size_t i = 0; i < policies.size(); ++i) {
    size_t found = policies[i].find(' ');
    if (found == std::string::npos) {
      LOG(INFO) << "Invalid value of directive: " << policies[i];
      return;
    }
  }
  csp_ = csp;
}

std::string XWalkBrowserContext::GetCSPString() const {
  return csp_;
}
#endif

void XWalkBrowserContext::InitVisitedLinkMaster() {
  visitedlink_master_.reset(
                            new visitedlink::VisitedLinkMaster(this, this, false));
  visitedlink_master_->Init();
}

void XWalkBrowserContext::AddVisitedURLs(const std::vector<GURL>& urls) {
  DCHECK(visitedlink_master_.get());
  visitedlink_master_->AddURLs(urls);
}

void XWalkBrowserContext::RebuildTable(
                                       const scoped_refptr<URLEnumerator>& enumerator) {
  // XWalkView rebuilds from XWalkWebChromeClient.getVisitedHistory. The client
  // can change in the lifetime of this XWalkView and may not yet be set here.
  // Therefore this initialization path is not used.
  enumerator->OnComplete(true);
}

// static
base::FilePath XWalkBrowserContext::GetCookieStorePath() {
  base::FilePath cookie_store_path;
  if (!base::PathService::Get(base::DIR_ANDROID_APP_DATA, &cookie_store_path)) {
    NOTREACHED() << "Failed to get app data directory for Android WebView";
  }
  cookie_store_path = cookie_store_path.Append(FILE_PATH_LITERAL("Cookies"));
  return cookie_store_path;
}

base::FilePath XWalkBrowserContext::GetCacheDir() {
  base::FilePath cache_path;
  if (!base::PathService::Get(base::DIR_CACHE, &cache_path)) {
    NOTREACHED() << "Failed to get app cache directory for Android WebView";
  }
  cache_path =
      cache_path.Append(FILE_PATH_LITERAL("com.pakdata.xwalk"));
  return cache_path;
}

network::mojom::NetworkContextParamsPtr XWalkBrowserContext::GetNetworkContextParams(
    bool in_memory, const base::FilePath& relative_partition_path) {
  network::mojom::NetworkContextParamsPtr context_params = network::mojom::NetworkContextParams::New();
  // TODO(iotto) : Fix
//  context_params->user_agent = xwalk::GetUserAgent();

  // TODO(ntfschr): set this value to a proper value based on the user's
  // preferred locales (http://crbug.com/898555). For now, set this to
  // "en-US,en" instead of "en-us,en", since Android guarantees region codes
  // will be uppercase.
  context_params->accept_language = net::HttpUtil::GenerateAcceptLanguageHeader("en-US,en");

  // HTTP cache
  context_params->http_cache_enabled = true;
  // TODO(iotto): Fix this
  context_params->http_cache_max_size = 20 * 1024 * 1024;  // 20M
      //GetHttpCacheSize();
  context_params->http_cache_path = GetCacheDir();

  // WebView should persist and restore cookies between app sessions (including
  // session cookies).
  context_params->cookie_path = XWalkBrowserContext::GetCookieStorePath();
  context_params->restore_old_session_cookies = true;
  context_params->persist_session_cookies = true;
  context_params->cookie_manager_params = network::mojom::CookieManagerParams::New();
  context_params->cookie_manager_params->allow_file_scheme_cookies = false;
      //CookieManager::GetInstance()->AllowFileSchemeCookies();

  context_params->initial_ssl_config = network::mojom::SSLConfig::New();
  // Allow SHA-1 to be used for locally-installed trust anchors, as WebView
  // should behave like the Android system would.
  context_params->initial_ssl_config->sha1_local_anchors_enabled = true;
  // Do not enforce the Legacy Symantec PKI policies outlined in
  // https://security.googleblog.com/2017/09/chromes-plan-to-distrust-symantec.html,
  // defer to the Android system.
  context_params->initial_ssl_config->symantec_enforcement_disabled = true;

  // WebView does not currently support Certificate Transparency
  // (http://crbug.com/921750).
  context_params->enforce_chrome_ct_policy = false;

  // WebView does not support ftp yet.
  context_params->enable_ftp_url_support = false;

  context_params->enable_brotli = true;
      //base::FeatureList::IsEnabled(android_webview::features::kWebViewBrotliSupport);

  context_params->check_clear_text_permitted = false;
      //AwContentBrowserClient::get_check_cleartext_permitted();

  // Update the cors_exempt_header_list to include internally-added headers, to
  // avoid triggering CORS checks.
  content::UpdateCorsExemptHeader(context_params.get());
  variations::UpdateCorsExemptHeaderForVariations(context_params.get());

  // Add proxy settings
//  AwProxyConfigMonitor::GetInstance()->AddProxyToNetworkContextParams(context_params);

  return context_params;
}
}  // namespace xwalk
