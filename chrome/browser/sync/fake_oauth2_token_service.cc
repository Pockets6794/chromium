// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/fake_oauth2_token_service.h"

#include "base/message_loop/message_loop.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"

void FakeOAuth2TokenService::FetchOAuth2Token(
    OAuth2TokenService::RequestImpl* request,
    const std::string& account_id,
    net::URLRequestContextGetter* getter,
    const std::string& client_id,
    const std::string& client_secret,
    const OAuth2TokenService::ScopeSet& scopes) {
  // Request will succeed without network IO.
  base::MessageLoop::current()->PostTask(FROM_HERE, base::Bind(
      &RequestImpl::InformConsumer,
      request->AsWeakPtr(),
      GoogleServiceAuthError(GoogleServiceAuthError::NONE),
      "access_token",
      base::Time::Max()));
}

void FakeOAuth2TokenService::UpdateCredentials(
    const std::string& account_id,
    const std::string& refresh_token) {
  account_id_ = account_id;
  refresh_token_ = refresh_token;
  FireRefreshTokenAvailable(account_id);
}

bool FakeOAuth2TokenService::RefreshTokenIsAvailable(
    const std::string& account_id) {
  return account_id_ == account_id && !refresh_token_.empty();
}

BrowserContextKeyedService* FakeOAuth2TokenService::BuildTokenService(
    content::BrowserContext* context) {
  Profile* profile = static_cast<Profile*>(context);

  FakeOAuth2TokenService* service = new FakeOAuth2TokenService();
  service->Initialize(profile);
  return service;
}
