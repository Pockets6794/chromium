// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/fast_show_pickler.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColorPriv.h"
#include "ui/app_list/app_list_item.h"
#include "ui/app_list/app_list_model.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/skia_util.h"

using app_list::AppListItem;
using app_list::AppListModel;

class AppListModelPicklerUnitTest : public testing::Test {
 protected:
  void CheckIsSame(AppListModel* m1, AppListModel* m2) {
    ASSERT_EQ(m1->item_list()->item_count(), m2->item_list()->item_count());
    for (size_t i = 0; i < m1->item_list()->item_count(); i++) {
      ASSERT_EQ(m1->item_list()->item_at(i)->id(),
                m2->item_list()->item_at(i)->id());
      ASSERT_EQ(m1->item_list()->item_at(i)->title(),
                m2->item_list()->item_at(i)->title());
      ASSERT_EQ(m1->item_list()->item_at(i)->full_name(),
                m2->item_list()->item_at(i)->full_name());
      CompareImages(m1->item_list()->item_at(i)->icon(),
                    m2->item_list()->item_at(i)->icon());
    }
  }

  void CompareImages(const gfx::ImageSkia& image1,
                     const gfx::ImageSkia& image2) {
    std::vector<gfx::ImageSkiaRep> reps1(image1.image_reps());
    std::vector<gfx::ImageSkiaRep> reps2(image2.image_reps());
    ASSERT_EQ(reps1.size(), reps2.size());
    for (size_t i = 0; i < reps1.size(); ++i) {
      ASSERT_TRUE(
          gfx::BitmapsAreEqual(reps1[i].sk_bitmap(), reps2[i].sk_bitmap()));
      ASSERT_EQ(reps1[i].scale(), reps2[i].scale());
    }
  }

  scoped_ptr<AppListModel> CopyViaPickle(AppListModel* model) {
    scoped_ptr<Pickle> pickle(
        FastShowPickler::PickleAppListModelForFastShow(model));
    return FastShowPickler::UnpickleAppListModelForFastShow(pickle.get());
  }

  void DoConsistencyChecks(AppListModel* model) {
    scoped_ptr<AppListModel> model2(CopyViaPickle(model));
    AppListModel dest_model;
    FastShowPickler::CopyOver(model2.get(), &dest_model);

    CheckIsSame(model, model2.get());
    CheckIsSame(model, &dest_model);
    CheckIsSame(model2.get(), &dest_model);
  }

  gfx::ImageSkia MakeImage() {
    const int kWidth = 10;
    const int kHeight = 10;
    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, kWidth, kHeight);
    bitmap.allocPixels();
    bitmap.eraseARGB(255, 1, 2, 3);
    return gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
  }
};

TEST_F(AppListModelPicklerUnitTest, EmptyModel) {
  AppListModel model;
  DoConsistencyChecks(&model);
}

TEST_F(AppListModelPicklerUnitTest, OneItem) {
  AppListModel model;
  AppListItem* app1 = new AppListItem("abc");
  app1->SetTitleAndFullName("ht", "hello, there");
  model.item_list()->AddItem(app1);

  DoConsistencyChecks(&model);
}

TEST_F(AppListModelPicklerUnitTest, TwoItems) {
  AppListModel model;
  AppListItem* app1 = new AppListItem("abc");
  app1->SetTitleAndFullName("ht", "hello, there");
  model.item_list()->AddItem(app1);

  AppListItem* app2 = new AppListItem("abc2");
  app2->SetTitleAndFullName("ht2", "hello, there 2");
  model.item_list()->AddItem(app2);

  DoConsistencyChecks(&model);
}

TEST_F(AppListModelPicklerUnitTest, Images) {
  AppListModel model;
  AppListItem* app1 = new AppListItem("abc");
  app1->SetTitleAndFullName("ht", "hello, there");
  app1->SetIcon(MakeImage(), true);
  model.item_list()->AddItem(app1);

  AppListItem* app2 = new AppListItem("abc2");
  app2->SetTitleAndFullName("ht2", "hello, there 2");
  model.item_list()->AddItem(app2);

  DoConsistencyChecks(&model);
}

TEST_F(AppListModelPicklerUnitTest, EmptyImage) {
  AppListModel model;
  AppListItem* app1 = new AppListItem("abc");
  app1->SetTitleAndFullName("ht", "hello, there");
  app1->SetIcon(gfx::ImageSkia(), true);
  model.item_list()->AddItem(app1);

  DoConsistencyChecks(&model);
}
