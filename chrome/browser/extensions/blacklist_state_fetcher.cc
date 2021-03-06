// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/blacklist_state_fetcher.h"

#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/safe_browsing/protocol_manager_helper.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/common/safe_browsing/crx_info.pb.h"
#include "google_apis/google_api_keys.h"
#include "net/base/escape.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace extensions {

BlacklistStateFetcher::BlacklistStateFetcher()
    : safe_browsing_config_initialized_(false),
      url_fetcher_id_(0) {
}

BlacklistStateFetcher::~BlacklistStateFetcher() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  STLDeleteContainerPairFirstPointers(requests_.begin(), requests_.end());
  requests_.clear();
}

void BlacklistStateFetcher::Request(const std::string& id,
                                    const RequestCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!safe_browsing_config_initialized_) {
    if (g_browser_process && g_browser_process->safe_browsing_service()) {
      SetSafeBrowsingConfig(
          g_browser_process->safe_browsing_service()->GetProtocolConfig());
    } else {
      // If safe browsing is not initialized, then it is probably turned off
      // completely.
      base::MessageLoopProxy::current()->PostTask(
          FROM_HERE, base::Bind(callback, BLACKLISTED_UNKNOWN));
      return;
    }
  }

  bool request_already_sent = ContainsKey(callbacks_, id);
  callbacks_.insert(std::make_pair(id, callback));
  if (request_already_sent)
    return;

  ClientCRXListInfoRequest request;

  request.set_id(id);
  std::string request_str;
  request.SerializeToString(&request_str);

  GURL request_url = RequestUrl();
  net::URLFetcher* fetcher = net::URLFetcher::Create(url_fetcher_id_++,
                                                     request_url,
                                                     net::URLFetcher::POST,
                                                     this);
  requests_[fetcher] = id;
  fetcher->SetAutomaticallyRetryOn5xx(false);  // Don't retry on error.
  if (g_browser_process && g_browser_process->safe_browsing_service()) {
    // Unless we are in unit test, safebrowsing service should be initialized.
    scoped_refptr<net::URLRequestContextGetter> request_context(
        g_browser_process->safe_browsing_service()->url_request_context());
    fetcher->SetRequestContext(request_context.get());
  }
  fetcher->SetUploadData("application/octet-stream", request_str);
  fetcher->Start();
}

void BlacklistStateFetcher::SetSafeBrowsingConfig(
    const SafeBrowsingProtocolConfig& config) {
  safe_browsing_config_ = config;
  safe_browsing_config_initialized_ = true;
}

GURL BlacklistStateFetcher::RequestUrl() const {
  std::string url = base::StringPrintf(
      "%s/%s?client=%s&appver=%s&pver=2.2",
      safe_browsing_config_.url_prefix.c_str(),
      "clientreport/crx-list-info",
      safe_browsing_config_.client_name.c_str(),
      safe_browsing_config_.version.c_str());
  std::string api_key = google_apis::GetAPIKey();
  if (!api_key.empty()) {
    base::StringAppendF(&url, "&key=%s",
                        net::EscapeQueryParamValue(api_key, true).c_str());
  }
  return GURL(url);
}

void BlacklistStateFetcher::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  std::map<const net::URLFetcher*, std::string>::iterator it =
     requests_.find(source);
  if (it == requests_.end()) {
    NOTREACHED();
    return;
  }

  scoped_ptr<const net::URLFetcher> fetcher;

  fetcher.reset(it->first);
  std::string id = it->second;
  requests_.erase(it);

  BlacklistState state;

  if (source->GetStatus().is_success() && source->GetResponseCode() == 200) {
    std::string data;
    source->GetResponseAsString(&data);
    ClientCRXListInfoResponse response;
    if (response.ParseFromString(data)) {
      state = static_cast<BlacklistState>(response.verdict());
    } else {
      state = BLACKLISTED_UNKNOWN;
    }
  } else {
    if (source->GetStatus().status() == net::URLRequestStatus::FAILED) {
      VLOG(1) << "Blacklist request for: " << id
              << " failed with error: " << source->GetStatus().error();
    } else {
      VLOG(1) << "Blacklist request for: " << id
              << " failed with error: " << source->GetResponseCode();
    }

    state = BLACKLISTED_UNKNOWN;
  }

  std::pair<CallbackMultiMap::iterator, CallbackMultiMap::iterator> range =
      callbacks_.equal_range(id);
  for (CallbackMultiMap::const_iterator callback_it = range.first;
       callback_it != range.second;
       ++callback_it) {
    callback_it->second.Run(state);
  }

  callbacks_.erase(range.first, range.second);
}

}  // namespace extensions

