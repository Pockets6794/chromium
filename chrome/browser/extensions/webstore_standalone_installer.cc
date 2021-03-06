// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/webstore_standalone_installer.h"

#include "base/values.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/extension_install_ui.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/webstore_data_fetcher.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension.h"
#include "url/gurl.h"

using content::WebContents;

namespace extensions {

const char kManifestKey[] = "manifest";
const char kIconUrlKey[] = "icon_url";
const char kLocalizedNameKey[] = "localized_name";
const char kLocalizedDescriptionKey[] = "localized_description";
const char kUsersKey[] = "users";
const char kShowUserCountKey[] = "show_user_count";
const char kAverageRatingKey[] = "average_rating";
const char kRatingCountKey[] = "rating_count";

const char kInvalidWebstoreItemId[] = "Invalid Chrome Web Store item ID";
const char kWebstoreRequestError[] =
    "Could not fetch data from the Chrome Web Store";
const char kInvalidWebstoreResponseError[] = "Invalid Chrome Web Store reponse";
const char kInvalidManifestError[] = "Invalid manifest";
const char kUserCancelledError[] = "User cancelled install";
const char kExtensionIsBlacklisted[] = "Extension is blacklisted";

WebstoreStandaloneInstaller::WebstoreStandaloneInstaller(
    const std::string& webstore_item_id,
    Profile* profile,
    const Callback& callback)
    : id_(webstore_item_id),
      callback_(callback),
      profile_(profile),
      install_source_(WebstoreInstaller::INSTALL_SOURCE_INLINE),
      show_user_count_(true),
      average_rating_(0.0),
      rating_count_(0) {
}

WebstoreStandaloneInstaller::~WebstoreStandaloneInstaller() {}

//
// Private interface implementation.
//

void WebstoreStandaloneInstaller::BeginInstall() {
  // Add a ref to keep this alive for WebstoreDataFetcher.
  // All code paths from here eventually lead to either CompleteInstall or
  // AbortInstall, which both release this ref.
  AddRef();

  if (!Extension::IdIsValid(id_)) {
    CompleteInstall(kInvalidWebstoreItemId);
    return;
  }

  // Use the requesting page as the referrer both since that is more correct
  // (it is the page that caused this request to happen) and so that we can
  // track top sites that trigger inline install requests.
  webstore_data_fetcher_.reset(new WebstoreDataFetcher(
      this,
      profile_->GetRequestContext(),
      GetRequestorURL(),
      id_));
  webstore_data_fetcher_->Start();
}

bool WebstoreStandaloneInstaller::CheckInstallValid(
    const base::DictionaryValue& manifest,
    std::string* error) {
  return true;
}

scoped_ptr<ExtensionInstallPrompt>
WebstoreStandaloneInstaller::CreateInstallUI() {
  return make_scoped_ptr(new ExtensionInstallPrompt(GetWebContents()));
}

scoped_ptr<WebstoreInstaller::Approval>
WebstoreStandaloneInstaller::CreateApproval() const {
  scoped_ptr<WebstoreInstaller::Approval> approval(
      WebstoreInstaller::Approval::CreateWithNoInstallPrompt(
          profile_,
          id_,
          scoped_ptr<base::DictionaryValue>(manifest_.get()->DeepCopy()),
          true));
  approval->skip_post_install_ui = !ShouldShowPostInstallUI();
  approval->use_app_installed_bubble = ShouldShowAppInstalledBubble();
  approval->installing_icon = gfx::ImageSkia::CreateFrom1xBitmap(icon_);
  return approval.Pass();
}

void WebstoreStandaloneInstaller::OnWebstoreRequestFailure() {
  CompleteInstall(kWebstoreRequestError);
}

void WebstoreStandaloneInstaller::OnWebstoreResponseParseSuccess(
    scoped_ptr<base::DictionaryValue> webstore_data) {
  if (!CheckRequestorAlive()) {
    CompleteInstall(std::string());
    return;
  }

  std::string error;

  if (!CheckInlineInstallPermitted(*webstore_data, &error)) {
    CompleteInstall(error);
    return;
  }

  if (!CheckRequestorPermitted(*webstore_data, &error)) {
    CompleteInstall(error);
    return;
  }

  // Manifest, number of users, average rating and rating count are required.
  std::string manifest;
  if (!webstore_data->GetString(kManifestKey, &manifest) ||
      !webstore_data->GetString(kUsersKey, &localized_user_count_) ||
      !webstore_data->GetDouble(kAverageRatingKey, &average_rating_) ||
      !webstore_data->GetInteger(kRatingCountKey, &rating_count_)) {
    CompleteInstall(kInvalidWebstoreResponseError);
    return;
  }

  // Optional.
  show_user_count_ = true;
  webstore_data->GetBoolean(kShowUserCountKey, &show_user_count_);

  if (average_rating_ < ExtensionInstallPrompt::kMinExtensionRating ||
      average_rating_ > ExtensionInstallPrompt::kMaxExtensionRating) {
    CompleteInstall(kInvalidWebstoreResponseError);
    return;
  }

  // Localized name and description are optional.
  if ((webstore_data->HasKey(kLocalizedNameKey) &&
      !webstore_data->GetString(kLocalizedNameKey, &localized_name_)) ||
      (webstore_data->HasKey(kLocalizedDescriptionKey) &&
      !webstore_data->GetString(
          kLocalizedDescriptionKey, &localized_description_))) {
    CompleteInstall(kInvalidWebstoreResponseError);
    return;
  }

  // Icon URL is optional.
  GURL icon_url;
  if (webstore_data->HasKey(kIconUrlKey)) {
    std::string icon_url_string;
    if (!webstore_data->GetString(kIconUrlKey, &icon_url_string)) {
      CompleteInstall(kInvalidWebstoreResponseError);
      return;
    }
    icon_url = GURL(extension_urls::GetWebstoreLaunchURL()).Resolve(
        icon_url_string);
    if (!icon_url.is_valid()) {
      CompleteInstall(kInvalidWebstoreResponseError);
      return;
    }
  }

  // Assume ownership of webstore_data.
  webstore_data_ = webstore_data.Pass();

  scoped_refptr<WebstoreInstallHelper> helper =
      new WebstoreInstallHelper(this,
                                id_,
                                manifest,
                                std::string(),  // We don't have any icon data.
                                icon_url,
                                profile_->GetRequestContext());
  // The helper will call us back via OnWebstoreParseSucces or
  // OnWebstoreParseFailure.
  helper->Start();
}

void WebstoreStandaloneInstaller::OnWebstoreResponseParseFailure(
    const std::string& error) {
  CompleteInstall(error);
}

void WebstoreStandaloneInstaller::OnWebstoreParseSuccess(
    const std::string& id,
    const SkBitmap& icon,
    base::DictionaryValue* manifest) {
  CHECK_EQ(id_, id);

  if (!CheckRequestorAlive()) {
    CompleteInstall(std::string());
    return;
  }

  manifest_.reset(manifest);
  icon_ = icon;

  std::string error;
  if (!CheckInstallValid(*manifest, &error)) {
    DCHECK(!error.empty());
    CompleteInstall(error);
    return;
  }

  install_prompt_ = CreateInstallPrompt();
  if (install_prompt_) {
    ShowInstallUI();
    // Control flow finishes up in InstallUIProceed or InstallUIAbort.
  } else {
    // Balanced in InstallUIAbort or indirectly in InstallUIProceed via
    // OnExtensionInstallSuccess or OnExtensionInstallFailure.
    AddRef();
    InstallUIProceed();
  }
}

void WebstoreStandaloneInstaller::OnWebstoreParseFailure(
    const std::string& id,
    InstallHelperResultCode result_code,
    const std::string& error_message) {
  CompleteInstall(error_message);
}

void WebstoreStandaloneInstaller::InstallUIProceed() {
  if (!CheckRequestorAlive()) {
    CompleteInstall(std::string());
    return;
  }

  ExtensionService* extension_service =
      ExtensionSystem::Get(profile_)->extension_service();
  const Extension* extension =
      extension_service->GetExtensionById(id_, true /* include disabled */);
  if (extension) {
    std::string install_result;  // Empty string for install success.
    if (!extension_service->IsExtensionEnabled(id_)) {
      if (!ExtensionPrefs::Get(profile_)->IsExtensionBlacklisted(id_)) {
        // If the extension is installed but disabled, and not blacklisted,
        // enable it.
        extension_service->EnableExtension(id_);
      } else {  // Don't install a blacklisted extension.
        install_result = kExtensionIsBlacklisted;
      }
    }  // else extension is installed and enabled; no work to be done.
    CompleteInstall(install_result);
    return;
  }

  scoped_ptr<WebstoreInstaller::Approval> approval = CreateApproval();

  scoped_refptr<WebstoreInstaller> installer = new WebstoreInstaller(
      profile_,
      this,
      &(GetWebContents()->GetController()),
      id_,
      approval.Pass(),
      install_source_);
  installer->Start();
}

void WebstoreStandaloneInstaller::InstallUIAbort(bool user_initiated) {
  CompleteInstall(kUserCancelledError);
  Release();  // Balanced in ShowInstallUI.
}

void WebstoreStandaloneInstaller::OnExtensionInstallSuccess(
    const std::string& id) {
  CHECK_EQ(id_, id);
  CompleteInstall(std::string());
  Release();  // Balanced in ShowInstallUI.
}

void WebstoreStandaloneInstaller::OnExtensionInstallFailure(
    const std::string& id,
    const std::string& error,
    WebstoreInstaller::FailureReason cancelled) {
  CHECK_EQ(id_, id);
  CompleteInstall(error);
  Release();  // Balanced in ShowInstallUI.
}

void WebstoreStandaloneInstaller::AbortInstall() {
  callback_.Reset();
  // Abort any in-progress fetches.
  if (webstore_data_fetcher_) {
    webstore_data_fetcher_.reset();
    Release();  // Matches the AddRef in BeginInstall.
  }
}

void WebstoreStandaloneInstaller::CompleteInstall(const std::string& error) {
  if (!callback_.is_null())
    callback_.Run(error.empty(), error);
  Release();  // Matches the AddRef in BeginInstall.
}

void
WebstoreStandaloneInstaller::ShowInstallUI() {
  std::string error;
  localized_extension_for_display_ =
      ExtensionInstallPrompt::GetLocalizedExtensionForDisplay(
          manifest_.get(),
          Extension::REQUIRE_KEY | Extension::FROM_WEBSTORE,
          id_,
          localized_name_,
          localized_description_,
          &error);
  if (!localized_extension_for_display_.get()) {
    CompleteInstall(kInvalidManifestError);
    return;
  }

  // Keep this alive as long as the install prompt lives.
  // Balanced in InstallUIAbort or indirectly in InstallUIProceed via
  // OnExtensionInstallSuccess or OnExtensionInstallFailure.
  AddRef();

  install_ui_ = CreateInstallUI();
  install_ui_->ConfirmStandaloneInstall(
      this, localized_extension_for_display_.get(), &icon_, *install_prompt_);
}

}  // namespace extensions
