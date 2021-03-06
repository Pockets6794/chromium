// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/drive/drive_app_registry.h"

#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "chrome/browser/drive/fake_drive_service.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "google_apis/drive/drive_api_parser.h"
#include "google_apis/drive/gdata_wapi_parser.h"
#include "google_apis/drive/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive {

class DriveAppRegistryTest : public testing::Test {
 protected:
  virtual void SetUp() OVERRIDE {
    fake_drive_service_.reset(new FakeDriveService);
    fake_drive_service_->LoadAppListForDriveApi("drive/applist.json");

    apps_registry_.reset(new DriveAppRegistry(fake_drive_service_.get()));
  }

  bool VerifyApp(const std::vector<DriveAppInfo>& list,
                 const std::string& app_id,
                 const std::string& app_name) {
    bool found = false;
    for (size_t i = 0; i < list.size(); ++i) {
      const DriveAppInfo& app = list[i];
      if (app_id == app.app_id) {
        EXPECT_EQ(app_name, app.app_name);
        found = true;
        break;
      }
    }
    EXPECT_TRUE(found) << "Unable to find app with app_id " << app_id;
    return found;
  }

  content::TestBrowserThreadBundle thread_bundle_;
  scoped_ptr<FakeDriveService> fake_drive_service_;
  scoped_ptr<DriveAppRegistry> apps_registry_;
};

TEST_F(DriveAppRegistryTest, LoadAndFindDriveApps) {
  apps_registry_->Update();
  base::RunLoop().RunUntilIdle();

  // Find by primary extension 'exe'.
  std::vector<DriveAppInfo> ext_results;
  base::FilePath ext_file(FILE_PATH_LITERAL("drive/file.exe"));
  apps_registry_->GetAppsForFile(ext_file.Extension(), "", &ext_results);
  ASSERT_EQ(1U, ext_results.size());
  VerifyApp(ext_results, "123456788192", "Drive app 1");

  // Find by primary MIME type.
  std::vector<DriveAppInfo> primary_app;
  apps_registry_->GetAppsForFile(base::FilePath::StringType(),
      "application/vnd.google-apps.drive-sdk.123456788192", &primary_app);
  ASSERT_EQ(1U, primary_app.size());
  VerifyApp(primary_app, "123456788192", "Drive app 1");

  // Find by secondary MIME type.
  std::vector<DriveAppInfo> secondary_app;
  apps_registry_->GetAppsForFile(
      base::FilePath::StringType(), "text/html", &secondary_app);
  ASSERT_EQ(1U, secondary_app.size());
  VerifyApp(secondary_app, "123456788192", "Drive app 1");
}

TEST_F(DriveAppRegistryTest, UpdateFromAppList) {
  scoped_ptr<base::Value> app_info_value =
      google_apis::test_util::LoadJSONFile("drive/applist.json");
  scoped_ptr<google_apis::AppList> app_list(
      google_apis::AppList::CreateFrom(*app_info_value));

  apps_registry_->UpdateFromAppList(*app_list);

  // Confirm that something was loaded from applist.json.
  std::vector<DriveAppInfo> ext_results;
  base::FilePath ext_file(FILE_PATH_LITERAL("drive/file.exe"));
  apps_registry_->GetAppsForFile(ext_file.Extension(), "", &ext_results);
  ASSERT_EQ(1U, ext_results.size());
}

TEST_F(DriveAppRegistryTest, MultipleUpdate) {
  // Call Update().
  apps_registry_->Update();

  // Call Update() again.
  // This call should be ignored because there is already an ongoing update.
  apps_registry_->Update();

  // The app list should be loaded only once.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, fake_drive_service_->app_list_load_count());
}

TEST(DriveAppRegistryUtilTest, FindPreferredIcon_Empty) {
  google_apis::InstalledApp::IconList icons;
  EXPECT_EQ("",
            util::FindPreferredIcon(icons, util::kPreferredIconSize).spec());
}

TEST(DriveAppRegistryUtilTest, FindPreferredIcon_) {
  const char kSmallerIconUrl[] = "http://example.com/smaller.png";
  const char kMediumIconUrl[] = "http://example.com/medium.png";
  const char kBiggerIconUrl[] = "http://example.com/bigger.png";
  const int kMediumSize = 16;

  google_apis::InstalledApp::IconList icons;
  // The icons are not sorted by the size.
  icons.push_back(std::make_pair(kMediumSize,
                                 GURL(kMediumIconUrl)));
  icons.push_back(std::make_pair(kMediumSize + 2,
                                 GURL(kBiggerIconUrl)));
  icons.push_back(std::make_pair(kMediumSize - 2,
                                 GURL(kSmallerIconUrl)));

  // Exact match.
  EXPECT_EQ(kMediumIconUrl,
            util::FindPreferredIcon(icons, kMediumSize).spec());
  // The requested size is in-between of smaller.png and
  // medium.png. medium.png should be returned.
  EXPECT_EQ(kMediumIconUrl,
            util::FindPreferredIcon(icons, kMediumSize - 1).spec());
  // The requested size is smaller than the smallest icon. The smallest icon
  // should be returned.
  EXPECT_EQ(kSmallerIconUrl,
            util::FindPreferredIcon(icons, kMediumSize - 3).spec());
  // The requested size is larger than the largest icon. The largest icon
  // should be returned.
  EXPECT_EQ(kBiggerIconUrl,
            util::FindPreferredIcon(icons, kMediumSize + 3).spec());
}

}  // namespace drive
