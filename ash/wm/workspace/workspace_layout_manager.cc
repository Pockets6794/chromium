// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/workspace_layout_manager.h"

#include "ash/display/display_controller.h"
#include "ash/root_window_controller.h"
#include "ash/screen_ash.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shell.h"
#include "ash/wm/always_on_top_controller.h"
#include "ash/wm/base_layout_manager.h"
#include "ash/wm/window_animations.h"
#include "ash/wm/window_positioner.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/base/ui_base_types.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/events/event.h"
#include "ui/views/corewm/window_util.h"

using aura::Window;

namespace ash {

namespace internal {

namespace {

// This specifies how much percent 30% of a window rect (width / height)
// must be visible when the window is added to the workspace.
const float kMinimumPercentOnScreenArea = 0.3f;

void MoveToDisplayForRestore(wm::WindowState* window_state) {
  if (!window_state->HasRestoreBounds())
    return;
  const gfx::Rect& restore_bounds = window_state->GetRestoreBoundsInScreen();

  // Move only if the restore bounds is outside of
  // the display. There is no information about in which
  // display it should be restored, so this is best guess.
  // TODO(oshima): Restore information should contain the
  // work area information like WindowResizer does for the
  // last window location.
  gfx::Rect display_area = Shell::GetScreen()->GetDisplayNearestWindow(
      window_state->window()).bounds();

  if (!display_area.Intersects(restore_bounds)) {
    DisplayController* display_controller =
        Shell::GetInstance()->display_controller();
    const gfx::Display& display =
        display_controller->GetDisplayMatching(restore_bounds);
    aura::Window* new_root =
        display_controller->GetRootWindowForDisplayId(display.id());
    if (new_root != window_state->window()->GetRootWindow()) {
      aura::Window* new_container =
          Shell::GetContainer(new_root, window_state->window()->parent()->id());
      new_container->AddChild(window_state->window());
    }
  }
}

}  // namespace

WorkspaceLayoutManager::WorkspaceLayoutManager(aura::Window* window)
    : BaseLayoutManager(window->GetRootWindow()),
      shelf_(NULL),
      window_(window),
      work_area_in_parent_(ScreenAsh::GetDisplayWorkAreaBoundsInParent(
          window->parent())),
      is_fullscreen_(GetRootWindowController(
          window->GetRootWindow())->GetWindowForFullscreenMode() != NULL) {
}

WorkspaceLayoutManager::~WorkspaceLayoutManager() {
}

void WorkspaceLayoutManager::SetShelf(internal::ShelfLayoutManager* shelf) {
  shelf_ = shelf;
}

void WorkspaceLayoutManager::OnWindowAddedToLayout(Window* child) {
  AdjustWindowBoundsWhenAdded(wm::GetWindowState(child));
  BaseLayoutManager::OnWindowAddedToLayout(child);
  UpdateShelfVisibility();
  UpdateFullscreenState();
  WindowPositioner::RearrangeVisibleWindowOnShow(child);
}

void WorkspaceLayoutManager::OnWillRemoveWindowFromLayout(Window* child) {
  BaseLayoutManager::OnWillRemoveWindowFromLayout(child);
  if (child->TargetVisibility())
    WindowPositioner::RearrangeVisibleWindowOnHideOrRemove(child);
}

void WorkspaceLayoutManager::OnWindowRemovedFromLayout(Window* child) {
  BaseLayoutManager::OnWindowRemovedFromLayout(child);
  UpdateShelfVisibility();
  UpdateFullscreenState();
}

void WorkspaceLayoutManager::OnChildWindowVisibilityChanged(Window* child,
                                                            bool visible) {
  BaseLayoutManager::OnChildWindowVisibilityChanged(child, visible);
  if (child->TargetVisibility()) {
    WindowPositioner::RearrangeVisibleWindowOnShow(child);
  } else {
    if (wm::GetWindowState(child)->IsFullscreen())
      UpdateFullscreenState();
    WindowPositioner::RearrangeVisibleWindowOnHideOrRemove(child);
  }
  UpdateShelfVisibility();
}

void WorkspaceLayoutManager::SetChildBounds(
    Window* child,
    const gfx::Rect& requested_bounds) {
  wm::WindowState* window_state = wm::GetWindowState(child);
  if (window_state->is_dragged()) {
    SetChildBoundsDirect(child, requested_bounds);
  } else if (!SetMaximizedOrFullscreenBounds(window_state)) {
    // Some windows rely on this to set their initial bounds.
    // Non-maximized/full-screen windows have their size constrained to the
    // work-area.
    gfx::Rect child_bounds(requested_bounds);
    child_bounds.set_width(std::min(work_area_in_parent_.width(),
                                    child_bounds.width()));
    child_bounds.set_height(std::min(work_area_in_parent_.height(),
                                     child_bounds.height()));
    AdjustSnappedBounds(window_state, &child_bounds);
    SetChildBoundsDirect(child, child_bounds);
  }
  UpdateShelfVisibility();
}

void WorkspaceLayoutManager::OnDisplayWorkAreaInsetsChanged() {
  const gfx::Rect work_area(ScreenAsh::GetDisplayWorkAreaBoundsInParent(
      window_->parent()));
  if (work_area != work_area_in_parent_) {
    AdjustAllWindowsBoundsForWorkAreaChange(
        ADJUST_WINDOW_WORK_AREA_INSETS_CHANGED);
  }
}

void WorkspaceLayoutManager::OnWindowPropertyChanged(Window* window,
                                                     const void* key,
                                                     intptr_t old) {
  if (key == aura::client::kAlwaysOnTopKey &&
      window->GetProperty(aura::client::kAlwaysOnTopKey)) {
    GetRootWindowController(window->GetRootWindow())->
        always_on_top_controller()->GetContainer(window)->AddChild(window);
  }
}

void WorkspaceLayoutManager::OnWindowStackingChanged(aura::Window* window) {
  BaseLayoutManager::OnWindowStackingChanged(window);
  UpdateShelfVisibility();
  UpdateFullscreenState();
}

void WorkspaceLayoutManager::OnWindowShowTypeChanged(
    wm::WindowState* window_state,
    wm::WindowShowType old_type) {
  if (old_type != wm::SHOW_TYPE_MINIMIZED &&
      !window_state->HasRestoreBounds() &&
      window_state->IsMaximizedOrFullscreen() &&
      !wm::IsMaximizedOrFullscreenWindowShowType(old_type)) {
    window_state->SaveCurrentBoundsForRestore();
  }
  // When restoring from a minimized state, we want to restore to the previous
  // bounds. However, we want to maintain the restore bounds. (The restore
  // bounds are set if a user maximized the window in one axis by double
  // clicking the window border for example).
  gfx::Rect restore;
  if (old_type == wm::SHOW_TYPE_MINIMIZED &&
      window_state->IsNormalShowState() &&
      window_state->HasRestoreBounds() &&
      !window_state->always_restores_to_restore_bounds()) {
    restore = window_state->GetRestoreBoundsInScreen();
    window_state->SaveCurrentBoundsForRestore();
  }

  UpdateBoundsFromShowType(window_state, old_type);
  ShowTypeChanged(window_state, old_type);

  if (window_state->IsNormalShowState())
    window_state->ClearRestoreBounds();

  // Set the restore rectangle to the previously set restore rectangle.
  if (!restore.IsEmpty())
    window_state->SetRestoreBoundsInScreen(restore);

  // Notify observers that fullscreen state may be changing.
  if (window_state->IsFullscreen() || old_type == wm::SHOW_TYPE_FULLSCREEN)
    UpdateFullscreenState();

  UpdateShelfVisibility();
}

void WorkspaceLayoutManager::AdjustAllWindowsBoundsForWorkAreaChange(
    AdjustWindowReason reason) {
  work_area_in_parent_ =
      ScreenAsh::GetDisplayWorkAreaBoundsInParent(window_->parent());
  BaseLayoutManager::AdjustAllWindowsBoundsForWorkAreaChange(reason);
}

void WorkspaceLayoutManager::AdjustWindowBoundsForWorkAreaChange(
    wm::WindowState* window_state,
    AdjustWindowReason reason) {
  if (window_state->is_dragged())
    return;

  // Do not cross fade here: the window's layer hierarchy may be messed up for
  // the transition between mirroring and extended. See also: crbug.com/267698
  // TODO(oshima): Differentiate display change and shelf visibility change, and
  // bring back CrossFade animation.
  if (window_state->IsMaximized() &&
      reason == ADJUST_WINDOW_WORK_AREA_INSETS_CHANGED) {
    SetChildBoundsDirect(window_state->window(),
                         ScreenAsh::GetMaximizedWindowBoundsInParent(
                             window_state->window()->parent()->parent()));
    return;
  }

  if (SetMaximizedOrFullscreenBounds(window_state))
    return;

  gfx::Rect bounds = window_state->window()->bounds();
  switch (reason) {
    case ADJUST_WINDOW_DISPLAY_SIZE_CHANGED:
      // The work area may be smaller than the full screen.  Put as much of the
      // window as possible within the display area.
      bounds.AdjustToFit(work_area_in_parent_);
      break;
    case ADJUST_WINDOW_WORK_AREA_INSETS_CHANGED:
      ash::wm::AdjustBoundsToEnsureMinimumWindowVisibility(
          work_area_in_parent_, &bounds);
      break;
  }
  AdjustSnappedBounds(window_state, &bounds);
  if (window_state->window()->bounds() != bounds)
    SetChildBoundsAnimated(window_state->window(), bounds);
}

void WorkspaceLayoutManager::AdjustWindowBoundsWhenAdded(
    wm::WindowState* window_state) {
  // Don't adjust window bounds if the bounds are empty as this
  // happens when a new views::Widget is created.
  // When a window is dragged and dropped onto a different
  // root window, the bounds will be updated after they are added
  // to the root window.
  if (window_state->window()->bounds().IsEmpty() ||
      window_state->is_dragged() ||
      SetMaximizedOrFullscreenBounds(window_state)) {
    return;
  }

  Window* window = window_state->window();
  gfx::Rect bounds = window->bounds();

  // Use entire display instead of workarea because the workarea can
  // be further shrunk by the docked area. The logic ensures 30%
  // visibility which should be enough to see where the window gets
  // moved.
  gfx::Rect display_area = ScreenAsh::GetDisplayBoundsInParent(window);

  int min_width = bounds.width() * kMinimumPercentOnScreenArea;
  int min_height = bounds.height() * kMinimumPercentOnScreenArea;
  ash::wm::AdjustBoundsToEnsureWindowVisibility(
      display_area, min_width, min_height, &bounds);
  AdjustSnappedBounds(window_state, &bounds);
  if (window->bounds() != bounds)
    window->SetBounds(bounds);
}

void WorkspaceLayoutManager::UpdateShelfVisibility() {
  if (shelf_)
    shelf_->UpdateVisibilityState();
}

void WorkspaceLayoutManager::UpdateFullscreenState() {
  bool is_fullscreen = GetRootWindowController(
      window_->GetRootWindow())->GetWindowForFullscreenMode() != NULL;
  if (is_fullscreen != is_fullscreen_) {
    ash::Shell::GetInstance()->NotifyFullscreenStateChange(
        is_fullscreen, window_->GetRootWindow());
    is_fullscreen_ = is_fullscreen;
  }
}

void WorkspaceLayoutManager::UpdateBoundsFromShowType(
    wm::WindowState* window_state,
    wm::WindowShowType old_show_type) {
  aura::Window* window = window_state->window();
  // See comment in SetMaximizedOrFullscreenBounds() as to why we use parent in
  // these calculation.
  if (window_state->IsMaximizedOrFullscreen())
    MoveToDisplayForRestore(window_state);

  wm::WindowShowType show_type = window_state->window_show_type();
  gfx::Rect bounds_in_parent;
  switch (show_type) {
    case wm::SHOW_TYPE_DEFAULT:
    case wm::SHOW_TYPE_NORMAL:
    case wm::SHOW_TYPE_LEFT_SNAPPED:
    case wm::SHOW_TYPE_RIGHT_SNAPPED:
      if (window_state->HasRestoreBounds())
        bounds_in_parent = window_state->GetRestoreBoundsInParent();
      else
        bounds_in_parent = window->bounds();
      // Make sure that part of the window is always visible.
      wm::AdjustBoundsToEnsureMinimumWindowVisibility(
          work_area_in_parent_, &bounds_in_parent);

      if (show_type == wm::SHOW_TYPE_LEFT_SNAPPED ||
          show_type == wm::SHOW_TYPE_RIGHT_SNAPPED) {
        AdjustSnappedBounds(window_state, &bounds_in_parent);
      } else {
        bounds_in_parent = BaseLayoutManager::BoundsWithScreenEdgeVisible(
            window->parent()->parent(),
            bounds_in_parent);
      }
      break;

    case wm::SHOW_TYPE_MAXIMIZED:
      bounds_in_parent = ScreenAsh::GetMaximizedWindowBoundsInParent(
          window->parent()->parent());
      break;

    case wm::SHOW_TYPE_FULLSCREEN:
      bounds_in_parent = ScreenAsh::GetDisplayBoundsInParent(
          window->parent()->parent());
      break;

    case wm::SHOW_TYPE_MINIMIZED:
    case wm::SHOW_TYPE_INACTIVE:
    case wm::SHOW_TYPE_DETACHED:
    case wm::SHOW_TYPE_END:
    case wm::SHOW_TYPE_AUTO_POSITIONED:
      return;
  }

  if (old_show_type == wm::SHOW_TYPE_MINIMIZED ||
      (window_state->IsFullscreen() &&
       !window_state->animate_to_fullscreen())) {
    SetChildBoundsDirect(window, bounds_in_parent);
  } else if (window_state->IsMaximizedOrFullscreen() ||
             IsMaximizedOrFullscreenWindowShowType(old_show_type)) {
    CrossFadeToBounds(window, bounds_in_parent);
  } else {
    SetChildBoundsAnimated(window, bounds_in_parent);
  }
}

bool WorkspaceLayoutManager::SetMaximizedOrFullscreenBounds(
    wm::WindowState* window_state) {
  DCHECK(!window_state->is_dragged());

  // During animations there is a transform installed on the workspace
  // windows. For this reason this code uses the parent so that the transform is
  // ignored.
  if (window_state->IsMaximized()) {
    SetChildBoundsDirect(
        window_state->window(), ScreenAsh::GetMaximizedWindowBoundsInParent(
            window_state->window()->parent()->parent()));
    return true;
  }
  if (window_state->IsFullscreen()) {
    SetChildBoundsDirect(
        window_state->window(),
        ScreenAsh::GetDisplayBoundsInParent(
            window_state->window()->parent()->parent()));
    return true;
  }
  return false;
}

void WorkspaceLayoutManager::AdjustSnappedBounds(wm::WindowState* window_state,
                                                 gfx::Rect* bounds) {
  if (window_state->is_dragged() || !window_state->IsSnapped())
    return;
  gfx::Rect maximized_bounds = ScreenAsh::GetMaximizedWindowBoundsInParent(
      window_state->window()->parent()->parent());
  if (window_state->window_show_type() == wm::SHOW_TYPE_LEFT_SNAPPED)
    bounds->set_x(maximized_bounds.x());
  else if (window_state->window_show_type() == wm::SHOW_TYPE_RIGHT_SNAPPED)
    bounds->set_x(maximized_bounds.right() - bounds->width());
  bounds->set_y(maximized_bounds.y());
  // TODO(varkha): Set width to 50% here for snapped windows.
  bounds->set_height(maximized_bounds.height());
}

void WorkspaceLayoutManager::SetChildBoundsAnimated(Window* child,
                                                    const gfx::Rect& bounds) {
  const int kBoundsChangeSlideDurationMs = 120;

  ui::Layer* layer = child->layer();
  ui::ScopedLayerAnimationSettings slide_settings(layer->GetAnimator());
  slide_settings.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  slide_settings.SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(kBoundsChangeSlideDurationMs));
  SetChildBoundsDirect(child, bounds);
}

}  // namespace internal
}  // namespace ash
