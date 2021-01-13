// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Copyright (c) 2013 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "xwalk/runtime/browser/runtime_url_request_context_getter.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/cookie_store_factory.h"
#include "content/public/browser/network_quality_observer_factory.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/ct_policy_enforcer.h"
#include "net/cert/ct_policy_status.h"
#include "net/cert/ct_verifier.h"
#include "net/cert/do_nothing_ct_verifier.h"
#include "net/cookies/cookie_monster.h"
#include "net/dns/host_resolver.h"
#include "net/dns/mapped_host_resolver.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties_impl.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
//#include "net/ssl/channel_id_service.h"
//#include "net/ssl/default_channel_id_store.h"
#include "net/ssl/ssl_config_service_defaults.h"
#include "net/url_request/data_protocol_handler.h"
#include "net/url_request/file_protocol_handler.h"
#include "net/url_request/static_http_user_agent_settings.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_context_storage.h"
#include "net/url_request/url_request_intercepting_job_factory.h"
#include "net/url_request/url_request_interceptor.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "xwalk/application/common/constants.h"
#include "xwalk/runtime/browser/runtime_network_delegate.h"
#include "xwalk/runtime/common/xwalk_content_client.h"
#include "xwalk/runtime/common/xwalk_switches.h"

#ifdef TENTA_CHROMIUM_BUILD
//#include "host_resolver_tenta.h"
#include "tenta_host_resolver.h"
#include "xwalk/third_party/tenta/chromium_cache/chromium_cache_factory.h"

namespace tenta_cache = tenta::fs::cache;
#endif
#include "meta_logging.h"

#if defined(OS_ANDROID)
#include "net/proxy_resolution/proxy_config_service_android.h"
#include "xwalk/runtime/browser/android/cookie_manager.h"
#include "xwalk/runtime/browser/android/net/android_protocol_handler.h"
#include "xwalk/runtime/browser/android/net/url_constants.h"
#include "xwalk/runtime/browser/android/net/xwalk_cookie_store_wrapper.h"
#include "xwalk/runtime/browser/android/net/xwalk_url_request_job_factory.h"
#include "xwalk/runtime/browser/android/xwalk_request_interceptor.h"
#endif

#include "net/nqe/network_quality_estimator.h"
#include "net/nqe/network_quality_estimator_params.h"
#include "net/nqe/rtt_throughput_estimates_observer.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/network_quality_observer_factory.h"
#include "services/network/public/cpp/features.h"

using content::BrowserThread;

namespace xwalk {

namespace {

// On apps targeting API level O or later, check cleartext is enforced.
bool g_check_cleartext_permitted = false; // TODO(iotto): Make configurable


const char kProxyServerSwitch[] = "proxy-server";
const char kProxyBypassListSwitch[] = "proxy-bypass-list";


// Field trial for network quality estimator. Seeds RTT and downstream
// throughput observations with values that correspond to the connection type
// determined by the operating system.
const char kNetworkQualityEstimatorFieldTrialName[] = "NetworkQualityEstimator";

// TODO(rakuco): should Crosswalk's release cycle ever align with Chromium's,
// we should use Chromium's Certificate Transparency policy and stop ignoring
// CT information with the classes below.
// See the discussion in http://crbug.com/669978 for more information.

// TODO replaced by net::DoNothingCTVerifier
/*// A CTVerifier which ignores Certificate Transparency information.
class IgnoresCTVerifier : public net::CTVerifier {
 public:
  IgnoresCTVerifier() = default;
  ~IgnoresCTVerifier() override = default;

  void Verify(X509Certificate* cert,
                      base::StringPiece stapled_ocsp_response,
                      base::StringPiece sct_list_from_tls_extension,
                      SignedCertificateTimestampAndStatusList* output_scts,
                      const NetLogWithSource& net_log) override {
    // TODO see net/cert/do_nothing_ct_verifier.cc
    // TODO check for nullptr
    output_scts->clear();
  }

  void SetObserver(Observer* observer) override {}
};
*/

// A CTPolicyEnforcer that accepts all certificates.
class IgnoresCTPolicyEnforcer : public net::CTPolicyEnforcer {
 public:
  IgnoresCTPolicyEnforcer() = default;
  ~IgnoresCTPolicyEnforcer() override = default;

  net::ct::CTPolicyCompliance CheckCompliance(
      net::X509Certificate* cert,
      const net::ct::SCTList& verified_scts,
      const net::NetLogWithSource& net_log) override {
    return net::ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS;
  }

};

}  // namespace

int GetDiskCacheSize() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  if (!command_line->HasSwitch(switches::kDiskCacheSize))
    return 0;

  std::string str_value = command_line->GetSwitchValueASCII(
      switches::kDiskCacheSize);

  int size = 0;
  if (!base::StringToInt(str_value, &size)) {
    LOG(ERROR) << "The value " << str_value
                  << " can not be converted to integer, ignoring!";
  }

  return size;
}

RuntimeURLRequestContextGetter::RuntimeURLRequestContextGetter(
    bool ignore_certificate_errors, const base::FilePath& base_path,
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner, 
    const scoped_refptr<base::SingleThreadTaskRunner>& file_task_runner,
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors)
    : ignore_certificate_errors_(ignore_certificate_errors),
      base_path_(base_path),
      io_task_runner_(io_task_runner),
      file_task_runner_(file_task_runner),
      request_interceptors_(std::move(request_interceptors)) {
  // Must first be created on the UI thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  std::swap(protocol_handlers_, *protocol_handlers);

//  // We must create the proxy config service on the UI loop on Linux because it
//  // must synchronously run on the glib message loop. This will be passed to
//  // the URLRequestContextStorage on the IO thread in GetURLRequestContext().
#if defined(OS_ANDROID)
  proxy_config_service_ = net::ProxyResolutionService::CreateSystemProxyConfigService(io_task_runner);
  net::ProxyConfigServiceAndroid* android_config_service =
      static_cast<net::ProxyConfigServiceAndroid*>(proxy_config_service_.get());
  android_config_service->set_exclude_pac_url(true);
#else
  proxy_config_service_ = net::ProxyService::CreateSystemProxyConfigService(
      io_task_runner, file_task_runner);
#endif
}

RuntimeURLRequestContextGetter::~RuntimeURLRequestContextGetter() {
}

net::URLRequestContext* RuntimeURLRequestContextGetter::GetURLRequestContext() {
  LOG(INFO) << "iotto " << __func__;
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (!url_request_context_) {
    url_request_context_.reset(new net::URLRequestContext());
    network_delegate_.reset(new RuntimeNetworkDelegate);
    url_request_context_->set_network_delegate(network_delegate_.get());
    url_request_context_->set_enable_brotli(true);

    // TODO(iotto): Make this configurable
    url_request_context_->set_check_cleartext_permitted(g_check_cleartext_permitted);

    std::map<std::string, std::string> network_quality_estimator_params;
    // TODO(iotto): Setup estimator network params
//    variations::GetVariationParams(kNetworkQualityEstimatorFieldTrialName,
//                                   &network_quality_estimator_params);
//
//    LOG(INFO) << "iotto " << __func__ << " params_size=" << network_quality_estimator_params.size();
//
//    network_quality_estimator_params["effective_connection_type_algorithm"] = "TransportRTTOrDownstreamThroughput";

    // TODO(iotto): Create on UI thread?!
    // see comment in content/browser/network_service_instance_impl.cc:182
//    network_quality_tracker_ = std::make_unique<network::NetworkQualityTracker>(
//        base::BindRepeating(&content::GetNetworkService));
//
//    network_quality_observer_ =
//        content::CreateNetworkQualityObserver(network_quality_tracker_.get());

    storage_.reset(
        new net::URLRequestContextStorage(url_request_context_.get()));
#if defined(OS_ANDROID)
    storage_->set_cookie_store(base::WrapUnique(new XWalkCookieStoreWrapper));
//    storage_->set_cookie_store(content::CreateCookieStore(content::CookieStoreConfig(), nullptr));
    LOG(WARNING) << "iotto " << __func__ << " set_network_quality_estimator!";
//    url_request_context_->set_network_quality_estimator(_network_quality_estimator.get());
#else
    content::CookieStoreConfig cookie_config(base_path_.Append(
            application::kCookieDatabaseFilename),
        content::CookieStoreConfig::PERSISTANT_SESSION_COOKIES,
        NULL, NULL);

    cookie_config.cookieable_schemes.insert(
        cookie_config.cookieable_schemes.begin(),
        net::CookieMonster::kDefaultCookieableSchemes,
        net::CookieMonster::kDefaultCookieableSchemes +
        net::CookieMonster::kDefaultCookieableSchemesCount);
    cookie_config.cookieable_schemes.push_back(application::kApplicationScheme);
    cookie_config.cookieable_schemes.push_back(content::kChromeDevToolsScheme);

    auto cookie_store = content::CreateCookieStore(cookie_config);
    storage_->set_cookie_store(std::move(cookie_store));
#endif
    storage_->set_http_user_agent_settings(
        base::WrapUnique(
            new net::StaticHttpUserAgentSettings("en-US,en",
                                                 xwalk::GetUserAgent())));

#ifdef TENTA_CHROMIUM_BUILD
    std::unique_ptr<tenta_cache::ChromiumCacheFactory> main_backend(
        new tenta_cache::ChromiumCacheFactory(nullptr));

//    base::FilePath cache_path = base_path_.Append(FILE_PATH_LITERAL("Cache"));
//    std::unique_ptr<net::HttpCache::DefaultBackend> main_backend(
//        new net::HttpCache::DefaultBackend(
//            net::DISK_CACHE, net::CACHE_BACKEND_DEFAULT, cache_path,
//            GetDiskCacheSize()));


    std::unique_ptr<net::HostResolver> host_resolver
    (new tenta::ext::TentaHostResolver());
//        = net::HostResolver::CreateStandaloneResolver(nullptr /*netlog*/);
#else
    base::FilePath cache_path = base_path_.Append(FILE_PATH_LITERAL("Cache"));

    std::unique_ptr<net::HttpCache::DefaultBackend> main_backend(
        new net::HttpCache::DefaultBackend(
            net::DISK_CACHE, net::CACHE_BACKEND_DEFAULT, cache_path,
            GetDiskCacheSize()));

    std::unique_ptr<net::MappedHostResolver> host_resolver(
        new net::MappedHostResolver(
            net::HostResolver::CreateDefaultResolver(nullptr)));
#endif

    storage_->set_cert_verifier(net::CertVerifier::CreateDefault(nullptr/*cert_net_fetcher*/));
    storage_->set_transport_security_state(
        base::WrapUnique(new net::TransportSecurityState));

    // We consciously ignore certificate transparency checks at the moment
    // because we risk ignoring valid logs or accepting unqualified logs since
    // Crosswalk's release schedule does not match Chromium's. Additionally,
    // all the CT verification mechanisms stop working 70 days after a build is
    // made, so we would also need to release more quickly and users would need
    // to update their apps as well.
    // See the discussion in http://crbug.com/669978 for more information.
    storage_->set_cert_transparency_verifier(
        base::WrapUnique(new net::DoNothingCTVerifier()));
    storage_->set_ct_policy_enforcer(
        base::WrapUnique(new IgnoresCTPolicyEnforcer));

    storage_->set_ssl_config_service(base::WrapUnique(new net::SSLConfigServiceDefaults()));
    storage_->set_http_auth_handler_factory(
        net::HttpAuthHandlerFactory::CreateDefault());
    storage_->set_http_server_properties(
        std::unique_ptr < net::HttpServerProperties
            > (new net::HttpServerPropertiesImpl));

    std::unique_ptr<net::ProxyResolutionService> pr_service = net::ProxyResolutionService::CreateDirect();
//    std::unique_ptr<net::ProxyResolutionService> pr_service = net::ProxyResolutionService::CreateWithoutProxyResolver(
//        std::move(proxy_config_service_), NULL);
    pr_service->set_quick_check_enabled(false);
    storage_->set_proxy_resolution_service(std::move(pr_service));

    net::HttpNetworkSession::Params network_session_params;
    net::HttpNetworkSession::Context network_session_context;

    network_session_context.cert_verifier = url_request_context_->cert_verifier();
    network_session_context.transport_security_state = url_request_context_->transport_security_state();
    network_session_context.cert_transparency_verifier = url_request_context_->cert_transparency_verifier();
    network_session_context.ct_policy_enforcer = url_request_context_->ct_policy_enforcer();
    network_session_context.ssl_config_service = url_request_context_->ssl_config_service();
    network_session_context.http_auth_handler_factory = url_request_context_->http_auth_handler_factory();
    network_session_context.http_server_properties = url_request_context_->http_server_properties();
    network_session_context.network_quality_estimator = url_request_context_->network_quality_estimator();

    network_session_params.ignore_certificate_errors = ignore_certificate_errors_;
    network_session_params.enable_quic = false;

    // Give |storage_| ownership at the end in case it's |mapped_host_resolver|.
    storage_->set_host_resolver(std::move(host_resolver));
    network_session_context.host_resolver = url_request_context_->host_resolver();
    network_session_context.proxy_resolution_service = url_request_context_->proxy_resolution_service();

    TENTA_LOG(WARNING) << __func__ << " GetAHold of NetworkSession to flush all sockets when mimic changes!";

    storage_->set_http_network_session(
        base::WrapUnique(new net::HttpNetworkSession(network_session_params, network_session_context)));
    storage_->set_http_transaction_factory(
        base::WrapUnique(
            new net::HttpCache(storage_->http_network_session(),
                               std::move(main_backend),
                               true /* is_main_cache; set_up_quic_server_info */)));

#if defined(OS_ANDROID)
    std::unique_ptr<XWalkURLRequestJobFactory> job_factory_impl(
        new XWalkURLRequestJobFactory);
#else
    std::unique_ptr<net::URLRequestJobFactoryImpl> job_factory_impl(
        new net::URLRequestJobFactoryImpl);
#endif

    bool set_protocol;

    // Step 1:
    // Install all the default schemes for crosswalk.
    for (content::ProtocolHandlerMap::iterator it = protocol_handlers_.begin();
        it != protocol_handlers_.end(); ++it) {
      set_protocol = job_factory_impl->SetProtocolHandler(
          it->first, base::WrapUnique(it->second.release()));
      DCHECK(set_protocol);
    }
    protocol_handlers_.clear();

    // Step 2:
    // Add new basic schemes.
    set_protocol = job_factory_impl->SetProtocolHandler(
        url::kDataScheme, base::WrapUnique(new net::DataProtocolHandler));
    DCHECK(set_protocol);
    set_protocol = job_factory_impl->SetProtocolHandler(
        url::kFileScheme, base::WrapUnique(new net::FileProtocolHandler(base::CreateSequencedTaskRunnerWithTraits( {
            base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN }))));
    DCHECK(set_protocol);

    // Step 3:
    // Add the scheme interceptors.
    // in the order in which they appear in the |request_interceptors| vector.
    typedef std::vector<net::URLRequestInterceptor*> URLRequestInterceptorVector;
    URLRequestInterceptorVector request_interceptors;

#if defined(OS_ANDROID)
    request_interceptors.push_back(
        CreateContentSchemeRequestInterceptor().release());
    request_interceptors.push_back(
        CreateAssetFileRequestInterceptor().release());
    request_interceptors.push_back(
        CreateAppSchemeRequestInterceptor().release());
    // The XWalkRequestInterceptor must come after the content and asset
    // file job factories. This for WebViewClassic compatibility where it
    // was not possible to intercept resource loads to resolvable content://
    // and file:// URIs.
    // This logical dependency is also the reason why the Content
    // ProtocolHandler has to be added as a ProtocolInterceptJobFactory rather
    // than via SetProtocolHandler.
    request_interceptors.push_back(new XWalkRequestInterceptor());
#endif

    // The chain of responsibility will execute the handlers in reverse to the
    // order in which the elements of the chain are created.
    std::unique_ptr<net::URLRequestJobFactory> job_factory(
        std::move(job_factory_impl));
    for (URLRequestInterceptorVector::reverse_iterator i = request_interceptors
        .rbegin(); i != request_interceptors.rend(); ++i) {
      job_factory.reset(
          new net::URLRequestInterceptingJobFactory(std::move(job_factory),
                                                    base::WrapUnique(*i)));
    }

    // Set up interceptors in the reverse order.
    std::unique_ptr<net::URLRequestJobFactory> top_job_factory = std::move(
        job_factory);
    for (content::URLRequestInterceptorScopedVector::reverse_iterator i =
        request_interceptors_.rbegin(); i != request_interceptors_.rend();
        ++i) {
      top_job_factory.reset(
          new net::URLRequestInterceptingJobFactory(std::move(top_job_factory),
                                                    std::move(*i)));
    }
    request_interceptors_.clear();

    storage_->set_job_factory(std::move(top_job_factory));
  }

  return url_request_context_.get();
}

scoped_refptr<base::SingleThreadTaskRunner> RuntimeURLRequestContextGetter::GetNetworkTaskRunner() const {
  return base::CreateSingleThreadTaskRunnerWithTraits({content::BrowserThread::IO});
}

net::HostResolver* RuntimeURLRequestContextGetter::host_resolver() {
  return url_request_context_->host_resolver();
}

void RuntimeURLRequestContextGetter::UpdateAcceptLanguages(
    const std::string& accept_languages) {
  if (!storage_)
    return;
  storage_->set_http_user_agent_settings(
      base::WrapUnique(
          new net::StaticHttpUserAgentSettings(accept_languages,
                                               xwalk::GetUserAgent())));
}

}  // namespace xwalk
