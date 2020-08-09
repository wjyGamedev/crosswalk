// Copyright 2013 The Chromium Authors. All rights reserved.
// Copyright 2015 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "xwalk/runtime/browser/devtools/xwalk_devtools_manager_delegate.h"

#include <string>
#include <vector>

#include "base/atomicops.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
//#include "components/devtools_discovery/basic_target_descriptor.h"
//#include "components/devtools_discovery/devtools_discovery_manager.h"
//#include "components/devtools_http_handler/devtools_http_handler.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_frontend_host.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/devtools_socket_factory.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/user_agent.h"
#include "content/shell/browser/shell.h"
#include "content/shell/common/shell_content_client.h"
#include "grit/xwalk_resources.h"
#include "net/base/net_errors.h"
#include "net/socket/tcp_server_socket.h"
#include "net/log/net_log_source.h"
#include "ui/base/resource/resource_bundle.h"
#include "xwalk/runtime/common/xwalk_content_client.h"
#include "xwalk/runtime/browser/runtime.h"
#include "xwalk/runtime/browser/xwalk_browser_context.h"

#ifdef TENTA_CHROMIUM_BUILD
#include "content/public/browser/devtools_external_agent_proxy.h"
#include "content/public/browser/devtools_external_agent_proxy_delegate.h"

// tenta
#include "browser/tenta_tab_model.h"
#include "browser/tenta_tab.h"
#include "common/tenta_zone_tab_id.h"

using namespace tenta::ext;
using content::DevToolsAgentHost;
using content::WebContents;
#endif

#include "meta_logging.h"

namespace xwalk {

namespace {

const int kBackLog = 10;
const char kLocalHost[] = "127.0.0.1";
base::subtle::Atomic32 g_last_used_port;

uint16_t GetInspectorPort() {
  const base::CommandLine& command_line = *base::CommandLine::ForCurrentProcess();
  // See if the user specified a port on the command line (useful for
  // automation). If not, use an ephemeral port by specifying 0.
  uint16_t port = 0;
  if (command_line.HasSwitch(switches::kRemoteDebuggingPort)) {
    int temp_port;
    std::string port_str = command_line.GetSwitchValueASCII(switches::kRemoteDebuggingPort);
    if (base::StringToInt(port_str, &temp_port) && temp_port > 0 && temp_port < 65535) {
      port = static_cast<uint16_t>(temp_port);
    } else {
      DLOG(WARNING) << "Invalid http debugger port number " << temp_port;
    }
  }
  return port;
}

class TCPServerSocketFactory :
    public content::DevToolsSocketFactory {
 public:
  TCPServerSocketFactory(const std::string& address, uint16_t port)
      : address_(address),
        port_(port) {
  }

 private:
  std::unique_ptr<net::ServerSocket> CreateForHttpServer() override {
    std::unique_ptr<net::ServerSocket> socket(new net::TCPServerSocket(nullptr, net::NetLogSource()));
    if (socket->ListenWithAddressAndPort(address_, port_, kBackLog) != net::OK)
      return std::unique_ptr<net::ServerSocket>();

    net::IPEndPoint endpoint;
    if (socket->GetLocalAddress(&endpoint) == net::OK)
      base::subtle::NoBarrier_Store(&g_last_used_port, endpoint.port());

    return socket;
  }

  std::unique_ptr<net::ServerSocket> CreateForTethering(std::string* name) override {
    return nullptr;
  }

  std::string address_;
  uint16_t port_;

  DISALLOW_COPY_AND_ASSIGN(TCPServerSocketFactory)
  ;
};

std::unique_ptr<content::DevToolsSocketFactory> CreateSocketFactory(uint16_t port) {
  return std::unique_ptr<content::DevToolsSocketFactory>(new TCPServerSocketFactory(kLocalHost, port));
}
/*
 scoped_refptr<content::DevToolsAgentHost>
 CreateNewShellTarget(XWalkBrowserContext* browser_context, const GURL& url) {
 Runtime* runtime = Runtime::Create(browser_context);
 return content::DevToolsAgentHost::GetOrCreateFor(runtime->web_contents());
 }

 class XWalkDevToolsDelegate :
 public devtools_http_handler::DevToolsHttpHandlerDelegate {
 public:
 explicit XWalkDevToolsDelegate(XWalkBrowserContext* browser_context);
 ~XWalkDevToolsDelegate() override;

 // devtools_http_handler::DevToolsHttpHandlerDelegate implementation.
 std::string GetDiscoveryPageHTML() override;
 std::string GetFrontendResource(const std::string& path) override;
 std::string GetPageThumbnailData(const GURL& url) override;
 content::DevToolsExternalAgentProxyDelegate*
 HandleWebSocketConnection(const std::string& path) override;

 private:
 XWalkBrowserContext* browser_context_;

 DISALLOW_COPY_AND_ASSIGN(XWalkDevToolsDelegate);
 };

 XWalkDevToolsDelegate::XWalkDevToolsDelegate(
 XWalkBrowserContext* browser_context)
 : browser_context_(browser_context) {
 devtools_discovery::DevToolsDiscoveryManager::GetInstance()->
 SetCreateCallback(base::Bind(&CreateNewShellTarget,
 base::Unretained(browser_context_)));
 }

 XWalkDevToolsDelegate::~XWalkDevToolsDelegate() {
 devtools_discovery::DevToolsDiscoveryManager::GetInstance()->
 SetCreateCallback(
 devtools_discovery::DevToolsDiscoveryManager::CreateCallback());
 }

 std::string XWalkDevToolsDelegate::GetDiscoveryPageHTML() {
 return ResourceBundle::GetSharedInstance().GetRawDataResource(
 IDR_DEVTOOLS_FRONTEND_PAGE_HTML).as_string();
 }

 std::string XWalkDevToolsDelegate::GetFrontendResource(
 const std::string& path) {
 return content::DevToolsFrontendHost::GetFrontendResource(path).as_string();
 }

 std::string XWalkDevToolsDelegate::GetPageThumbnailData(const GURL& url) {
 return std::string();
 }

 content::DevToolsExternalAgentProxyDelegate*
 XWalkDevToolsDelegate::HandleWebSocketConnection(const std::string& path) {
 return nullptr;
 }
 */

#ifdef TENTA_CHROMIUM_BUILD
/**
 *
 */
class ClientProxy :
    public content::DevToolsAgentHostClient {
 public:
  explicit ClientProxy(content::DevToolsExternalAgentProxy* proxy)
      : proxy_(proxy) {
  }
  ~ClientProxy() override {
  }

  void DispatchProtocolMessage(content::DevToolsAgentHost* agent_host, const std::string& message) override {
    proxy_->DispatchOnClientHost(message);
  }

  void AgentHostClosed(DevToolsAgentHost* agent_host) override {
    proxy_->ConnectionClosed();
  }

 private:
  content::DevToolsExternalAgentProxy* proxy_;DISALLOW_COPY_AND_ASSIGN(ClientProxy)
  ;
};

/**
 *
 */
class TabProxyDelegate :
    public content::DevToolsExternalAgentProxyDelegate {
 public:
  explicit TabProxyDelegate(TentaTab* tab, content::BrowserContext* context)
      : _tab_id(tab->zone_tab_id()),
        title_(tab->GetTitle()),
        url_(tab->GetURL()),
        _browser_context(context),
        agent_host_(tab->web_contents() ? DevToolsAgentHost::GetOrCreateFor(tab->web_contents()) : nullptr) {
  }

  ~TabProxyDelegate() override {
  }

  void Attach(content::DevToolsExternalAgentProxy* proxy) override {
    proxies_[proxy].reset(new ClientProxy(proxy));
    MaterializeAgentHost();
    if (agent_host_)
      agent_host_->AttachClient(proxies_[proxy].get());
  }

  void Detach(content::DevToolsExternalAgentProxy* proxy) override {
    auto it = proxies_.find(proxy);
    if (it == proxies_.end())
      return;
    if (agent_host_)
      agent_host_->DetachClient(it->second.get());
    proxies_.erase(it);
    if (proxies_.empty())
      agent_host_ = nullptr;
  }

  std::string GetType() override {
    return agent_host_ ? agent_host_->GetType() : DevToolsAgentHost::kTypePage;
  }

  std::string GetTitle() override {
    return agent_host_ ? agent_host_->GetTitle() : title_;
  }

  std::string GetDescription() override {
    return agent_host_ ? agent_host_->GetDescription() : "";
  }

  GURL GetURL() override {
    return agent_host_ ? agent_host_->GetURL() : url_;
  }

  GURL GetFaviconURL() override {
    return agent_host_ ? agent_host_->GetFaviconURL() : GURL();
  }

  std::string GetFrontendURL() override {
    return std::string();
  }

  bool Activate() override {
    TENTA_LOG_NET(WARNING) << __func__ << " TODO Implement!";
//    TabModel* model;
//    int index;
//    if (!FindTab(&model, &index))
//      return false;
//    model->SetActiveIndex(index);
    return true;
  }

  void Reload() override {
    MaterializeAgentHost();
    if (agent_host_)
      agent_host_->Reload();
  }

  bool Close() override {
    TENTA_LOG_NET(WARNING) << __func__ << " TODO Implement!";
//    TabModel* model;
//    int index;
//    if (!FindTab(&model, &index))
//      return false;
//    model->CloseTabAt(index);
    return true;
  }

  base::TimeTicks GetLastActivityTime() override {
    return agent_host_ ? agent_host_->GetLastActivityTime() : base::TimeTicks();
  }

  void SendMessageToBackend(content::DevToolsExternalAgentProxy* proxy, const std::string& message) override {
    auto it = proxies_.find(proxy);
    if (it == proxies_.end()) {
      TENTA_LOG_NET(ERROR) << __func__ << " NO_proxy_found";
      return;
    }
    if (agent_host_) {
      agent_host_->DispatchProtocolMessage(it->second.get(), message);
    } else {
      TENTA_LOG_NET(ERROR) << __func__ << " NULL_agent_host";
    }
  }

 private:
  void MaterializeAgentHost() {
    if (agent_host_)
      return;
    TentaTab* tab;
    if (!FindTab(&tab))
      return;
    WebContents* web_contents = tab->web_contents();
    if (!web_contents)
      return;
    agent_host_ = DevToolsAgentHost::GetOrCreateFor(web_contents);
  }

  bool FindTab(TentaTab** tab_out) const {
    TentaTabModel* tab_model = TentaTabModelFactory::GetForContext(_browser_context);
    if (tab_model) {
      for (TentaTabModel::const_iterator iter = tab_model->begin(); iter != tab_model->end(); ++iter) {
        TentaTab* tab = iter->second;

        if (tab->zone_tab_id() == _tab_id) {
          *tab_out = tab;
          return true;
        }
      }
    }
    return false;
  }

  TentaZoneTabID _tab_id;
  const std::string title_;
  const GURL url_;
  content::BrowserContext* _browser_context;
  scoped_refptr<DevToolsAgentHost> agent_host_;
  std::map<content::DevToolsExternalAgentProxy*, std::unique_ptr<ClientProxy>> proxies_;
  DISALLOW_COPY_AND_ASSIGN(TabProxyDelegate);
};

/**
 *
 */
scoped_refptr<DevToolsAgentHost> DevToolsAgentHostForTab(TentaTab* tab, content::BrowserContext* context) {
  scoped_refptr<DevToolsAgentHost> result = tab->GetDevToolsAgentHost();
  if (result)
    return result;

  result = DevToolsAgentHost::Forward(tab->zone_tab_id().ToString(), std::make_unique<TabProxyDelegate>(tab, context));
  tab->SetDevToolsAgentHost(result);
  return result;
}
#endif
}  // namespace

XWalkDevToolsManagerDelegate::XWalkDevToolsManagerDelegate(XWalkBrowserContext* browserContext)
    : _browser_context(browserContext) {
}

// static
int XWalkDevToolsManagerDelegate::GetHttpHandlerPort() {
  return base::subtle::NoBarrier_Load(&g_last_used_port);
}

//static
void XWalkDevToolsManagerDelegate::StartHttpHandler(XWalkBrowserContext* browserContext) {
  std::string frontend_url;
  /*
   #if defined(OS_ANDROID)
   frontend_url = base::StringPrintf(kFrontEndURL, GetWebKitRevision().c_str());
   #endif
   */
  content::DevToolsAgentHost::StartRemoteDebuggingServer(CreateSocketFactory(GetInspectorPort()),
                                                         browserContext->GetPath(), base::FilePath());
}

// static
void XWalkDevToolsManagerDelegate::StopHttpHandler() {
  content::DevToolsAgentHost::StopRemoteDebuggingServer();
}

scoped_refptr<content::DevToolsAgentHost> XWalkDevToolsManagerDelegate::CreateNewTarget(const GURL& url) {
  Runtime* runtime = Runtime::Create(_browser_context);
  return content::DevToolsAgentHost::GetOrCreateFor(runtime->web_contents());
}

content::DevToolsAgentHost::List XWalkDevToolsManagerDelegate::RemoteDebuggingTargets() {
  // Enumerate existing tabs, including the ones with no WebContents.
  content::DevToolsAgentHost::List result;

#ifdef TENTA_CHROMIUM_BUILD
  std::set<WebContents*> tab_web_contents;

  TentaTabModel* tab_model = TentaTabModelFactory::GetForContext(_browser_context);
  if (tab_model) {
    for (TentaTabModel::const_iterator iter = tab_model->begin(); iter != tab_model->end(); ++iter) {
      TentaTab* tab = iter->second;

      if (tab->web_contents()) {
        tab_web_contents.insert(tab->web_contents());
      }
      result.push_back(DevToolsAgentHostForTab(tab, _browser_context));
    }
  } else {
    TENTA_LOG_NET(ERROR) << __func__ << " NULL_tab_model";
  }

  // Add descriptors for targets not associated with any tabs.
  DevToolsAgentHost::List agents = DevToolsAgentHost::GetOrCreateAll();
  for (DevToolsAgentHost::List::iterator it = agents.begin(); it != agents.end(); ++it) {
    if (WebContents* web_contents = (*it)->GetWebContents()) {
      if (tab_web_contents.find(web_contents) != tab_web_contents.end())
        continue;
    }
    result.push_back(*it);
  }
#else
  result = content::DevToolsAgentHost::GetOrCreateAll();
#endif
    return result;
}

std::string XWalkDevToolsManagerDelegate::GetTargetType(content::WebContents* web_contents) {
  return content::DevToolsAgentHost::kTypePage;
}

std::string XWalkDevToolsManagerDelegate::GetTargetDescription(content::WebContents* web_contents) {
  return std::string();
//  android_webview::BrowserViewRenderer* bvr = android_webview::BrowserViewRenderer::FromWebContents(web_contents);
//  if (!bvr)
//    return "";
//  base::DictionaryValue description;
//  description.SetBoolean("attached", bvr->attached_to_window());
//  description.SetBoolean("visible", bvr->IsVisible());
//  gfx::Rect screen_rect = bvr->GetScreenRect();
//  description.SetInteger("screenX", screen_rect.x());
//  description.SetInteger("screenY", screen_rect.y());
//  description.SetBoolean("empty", screen_rect.size().IsEmpty());
//  if (!screen_rect.size().IsEmpty()) {
//    description.SetInteger("width", screen_rect.width());
//    description.SetInteger("height", screen_rect.height());
//  }
//  std::string json;
//  base::JSONWriter::Write(description, &json);
//  return json;
}

std::string XWalkDevToolsManagerDelegate::GetDiscoveryPageHTML() {
  return ui::ResourceBundle::GetSharedInstance().GetRawDataResource(
  IDR_DEVTOOLS_FRONTEND_PAGE_HTML).as_string();
}

bool XWalkDevToolsManagerDelegate::IsBrowserTargetDiscoverable() {
  return true;
}

//bool XWalkDevToolsManagerDelegate::HandleCommand(
//    DevToolsAgentHost* agent_host,
//    int session_id,
//    base::DictionaryValue* command_dict) {
//  /*
//   * TODO(iotto): for implementation details (like scroll, click, etc ...) see:
//   * chrome/browser/devtools/chrome_devtools_session.h
//   * chrome/browser/devtools/chrome_devtools_manager_delegate.cc
//   */
//  return false;
//}

XWalkDevToolsManagerDelegate::~XWalkDevToolsManagerDelegate() {
}

}  // namespace xwalk
