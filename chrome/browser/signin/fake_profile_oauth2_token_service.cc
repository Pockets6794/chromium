// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/fake_profile_oauth2_token_service.h"
#include "chrome/browser/signin/signin_account_id_helper.h"

FakeProfileOAuth2TokenService::PendingRequest::PendingRequest() {
}

FakeProfileOAuth2TokenService::PendingRequest::~PendingRequest() {
}

// static
BrowserContextKeyedService* FakeProfileOAuth2TokenService::Build(
    content::BrowserContext* profile) {
  FakeProfileOAuth2TokenService* service = new FakeProfileOAuth2TokenService();
  service->Initialize(reinterpret_cast<Profile*>(profile));
  return service;
}

FakeProfileOAuth2TokenService::FakeProfileOAuth2TokenService() {
  SigninAccountIdHelper::SetDisableForTest(true);
}

FakeProfileOAuth2TokenService::~FakeProfileOAuth2TokenService() {
  SigninAccountIdHelper::SetDisableForTest(false);
}

bool FakeProfileOAuth2TokenService::RefreshTokenIsAvailable(
    const std::string& account_id) {
  return !GetRefreshToken(account_id).empty();
}

std::vector<std::string> FakeProfileOAuth2TokenService::GetAccounts() {
  std::vector<std::string> account_ids;
  for (std::map<std::string, std::string>::const_iterator iter =
           refresh_tokens_.begin(); iter != refresh_tokens_.end(); ++iter) {
    account_ids.push_back(iter->first);
  }
  return account_ids;
}

void FakeProfileOAuth2TokenService::UpdateCredentials(
    const std::string& account_id,
    const std::string& refresh_token) {
  IssueRefreshTokenForUser(account_id, refresh_token);
}

void FakeProfileOAuth2TokenService::IssueRefreshToken(
    const std::string& token) {
  IssueRefreshTokenForUser("account_id", token);
}

void FakeProfileOAuth2TokenService::IssueRefreshTokenForUser(
    const std::string& account_id,
    const std::string& token) {
  if (token.empty()) {
    refresh_tokens_.erase(account_id);
    FireRefreshTokenRevoked(account_id);
  } else {
    refresh_tokens_[account_id] = token;
    FireRefreshTokenAvailable(account_id);
    // TODO(atwilson): Maybe we should also call FireRefreshTokensLoaded() here?
  }
}

void FakeProfileOAuth2TokenService::IssueAllTokensForAccount(
    const std::string& account_id,
    const std::string& access_token,
    const base::Time& expiration) {
  CompleteRequests(account_id,
                   true,
                   ScopeSet(),
                   GoogleServiceAuthError::AuthErrorNone(),
                   access_token,
                   expiration);
}

void FakeProfileOAuth2TokenService::IssueTokenForScope(
    const ScopeSet& scope,
    const std::string& access_token,
    const base::Time& expiration) {
  CompleteRequests("",
                   false,
                   scope,
                   GoogleServiceAuthError::AuthErrorNone(),
                   access_token,
                   expiration);
}

void FakeProfileOAuth2TokenService::IssueErrorForScope(
    const ScopeSet& scope,
    const GoogleServiceAuthError& error) {
  CompleteRequests("", false, scope, error, std::string(), base::Time());
}

void FakeProfileOAuth2TokenService::IssueErrorForAllPendingRequests(
    const GoogleServiceAuthError& error) {
  CompleteRequests("", true, ScopeSet(), error, std::string(), base::Time());
}

void FakeProfileOAuth2TokenService::IssueTokenForAllPendingRequests(
    const std::string& access_token,
    const base::Time& expiration) {
  CompleteRequests("",
                   true,
                   ScopeSet(),
                   GoogleServiceAuthError::AuthErrorNone(),
                   access_token,
                   expiration);
}

void FakeProfileOAuth2TokenService::CompleteRequests(
    const std::string& account_id,
    bool all_scopes,
    const ScopeSet& scope,
    const GoogleServiceAuthError& error,
    const std::string& access_token,
    const base::Time& expiration) {
  std::vector<FakeProfileOAuth2TokenService::PendingRequest> requests =
      GetPendingRequests();

  // Walk the requests and notify the callbacks.
  for (std::vector<PendingRequest>::iterator it = pending_requests_.begin();
       it != pending_requests_.end(); ++it) {
    if (!it->request)
      continue;

    bool scope_matches = all_scopes || it->scopes == scope;
    bool account_matches = account_id.empty() || account_id == it->account_id;
    if (account_matches && scope_matches)
      it->request->InformConsumer(error, access_token, expiration);
  }
}

std::string FakeProfileOAuth2TokenService::GetRefreshToken(
    const std::string& account_id) {
  return refresh_tokens_.count(account_id) > 0 ? refresh_tokens_[account_id] :
      std::string();
}

std::vector<FakeProfileOAuth2TokenService::PendingRequest>
FakeProfileOAuth2TokenService::GetPendingRequests() {
  std::vector<PendingRequest> valid_requests;
  for (std::vector<PendingRequest>::iterator it = pending_requests_.begin();
       it != pending_requests_.end(); ++it) {
    if (it->request)
      valid_requests.push_back(*it);
  }
  return valid_requests;
}

void FakeProfileOAuth2TokenService::FetchOAuth2Token(
    RequestImpl* request,
    const std::string& account_id,
    net::URLRequestContextGetter* getter,
    const std::string& client_id,
    const std::string& client_secret,
    const ScopeSet& scopes) {
  PendingRequest pending_request;
  pending_request.account_id = account_id;
  pending_request.client_id = client_id;
  pending_request.client_secret = client_secret;
  pending_request.scopes = scopes;
  pending_request.request = request->AsWeakPtr();
  pending_requests_.push_back(pending_request);
}

void FakeProfileOAuth2TokenService::InvalidateOAuth2Token(
    const std::string& account_id,
    const std::string& client_id,
    const ScopeSet& scopes,
    const std::string& access_token) {
  // Do nothing, as we don't have a cache from which to remove the token.
}
