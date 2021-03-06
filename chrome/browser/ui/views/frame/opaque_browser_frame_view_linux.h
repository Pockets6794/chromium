// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_OPAQUE_BROWSER_FRAME_VIEW_LINUX_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_OPAQUE_BROWSER_FRAME_VIEW_LINUX_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/ui/views/frame/opaque_browser_frame_view_platform_specific.h"
#include "ui/views/linux_ui/window_button_order_observer.h"

// Plumbs button change events from views::LinuxUI to
// OpaqueBrowserFrameViewLayout.
class OpaqueBrowserFrameViewLinux
    : public OpaqueBrowserFrameViewPlatformSpecific,
      public views::WindowButtonOrderObserver {
 public:
  OpaqueBrowserFrameViewLinux(
      OpaqueBrowserFrameView* view,
      OpaqueBrowserFrameViewLayout* layout);
  virtual ~OpaqueBrowserFrameViewLinux();

  // Overridden from OpaqueBrowserFrameViewPlatformSpecific:
  virtual bool ShouldShowCaptionButtons() const OVERRIDE;
  virtual bool ShouldShowTitleBar() const OVERRIDE;

  // Overridden from views::WindowButtonOrderObserver:
  virtual void OnWindowButtonOrderingChange(
      const std::vector<views::FrameButton>& leading_buttons,
      const std::vector<views::FrameButton>& trailing_buttons) OVERRIDE;

 private:
  OpaqueBrowserFrameView* view_;
  OpaqueBrowserFrameViewLayout* layout_;

  DISALLOW_COPY_AND_ASSIGN(OpaqueBrowserFrameViewLinux);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_OPAQUE_BROWSER_FRAME_VIEW_LINUX_H_
