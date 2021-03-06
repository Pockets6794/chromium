// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_CONTENTS_LAYOUT_MANAGER_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_CONTENTS_LAYOUT_MANAGER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/gfx/insets.h"
#include "ui/views/layout/layout_manager.h"

// ContentsLayoutManager positions the WebContents and devtools WebContents.
class ContentsLayoutManager : public views::LayoutManager {
 public:
  ContentsLayoutManager(views::View* devtools_view, views::View* contents_view);
  virtual ~ContentsLayoutManager();

  // Sets the active top margin; both devtools_view and contents_view will be
  // pushed down vertically by |margin|.
  void SetActiveTopMargin(int margin);

  // Sets the contents insets from the sides.
  void SetContentsViewInsets(const gfx::Insets& insets);

  // views::LayoutManager overrides:
  virtual void Layout(views::View* host) OVERRIDE;
  virtual gfx::Size GetPreferredSize(views::View* host) OVERRIDE;
  virtual void Installed(views::View* host) OVERRIDE;
  virtual void Uninstalled(views::View* host) OVERRIDE;

 private:
  views::View* devtools_view_;
  views::View* contents_view_;

  views::View* host_;

  gfx::Insets insets_;
  int active_top_margin_;

  DISALLOW_COPY_AND_ASSIGN(ContentsLayoutManager);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_CONTENTS_LAYOUT_MANAGER_H_
