// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/tab_contents/render_view_context_menu.h"
#include "chrome/browser/tab_contents/render_view_context_menu_browsertest_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

// GTK requires a X11-level mouse event to open a context menu correctly.
#if defined(TOOLKIT_GTK)
#define MAYBE_ContextMenuOrigin DISABLED_ContextMenuOrigin
#define MAYBE_HttpsContextMenuOrigin DISABLED_HttpsContextMenuOrigin
#define MAYBE_ContextMenuRedirect DISABLED_ContextMenuRedirect
#define MAYBE_HttpsContextMenuRedirect DISABLED_HttpsContextMenuRedirect
#else
#define MAYBE_ContextMenuOrigin ContextMenuOrigin
#define MAYBE_HttpsContextMenuOrigin HttpsContextMenuOrigin
#define MAYBE_ContextMenuRedirect ContextMenuRedirect
#define MAYBE_HttpsContextMenuRedirect HttpsContextMenuRedirect
#endif

namespace {

const base::FilePath::CharType kDocRoot[] =
    FILE_PATH_LITERAL("chrome/test/data/referrer_policy");

}  // namespace

class ReferrerPolicyTest : public InProcessBrowserTest {
 public:
   ReferrerPolicyTest() {}
   virtual ~ReferrerPolicyTest() {}

   virtual void SetUp() OVERRIDE {
     test_server_.reset(new net::SpawnedTestServer(
                            net::SpawnedTestServer::TYPE_HTTP,
                            net::SpawnedTestServer::kLocalhost,
                            base::FilePath(kDocRoot)));
     ASSERT_TRUE(test_server_->Start());
     ssl_test_server_.reset(new net::SpawnedTestServer(
                                net::SpawnedTestServer::TYPE_HTTPS,
                                net::SpawnedTestServer::kLocalhost,
                                base::FilePath(kDocRoot)));
     ASSERT_TRUE(ssl_test_server_->Start());

     InProcessBrowserTest::SetUp();
   }

 protected:
  enum ExpectedReferrer {
    EXPECT_EMPTY_REFERRER,
    EXPECT_FULL_REFERRER,
    EXPECT_ORIGIN_AS_REFERRER
  };

  // Returns the expected title for the tab with the given (full) referrer and
  // the expected modification of it.
  base::string16 GetExpectedTitle(const GURL& url,
                            ExpectedReferrer expected_referrer) {
    std::string referrer;
    switch (expected_referrer) {
      case EXPECT_EMPTY_REFERRER:
        referrer = "Referrer is empty";
        break;
      case EXPECT_FULL_REFERRER:
        referrer = "Referrer is " + url.spec();
        break;
      case EXPECT_ORIGIN_AS_REFERRER:
        referrer = "Referrer is " + url.GetWithEmptyPath().spec();
        break;
    }
    return base::ASCIIToUTF16(referrer);
  }

  // Adds all possible titles to the TitleWatcher, so we don't time out
  // waiting for the title if the test fails.
  void AddAllPossibleTitles(const GURL& url,
                            content::TitleWatcher* title_watcher) {
    title_watcher->AlsoWaitForTitle(
        GetExpectedTitle(url, EXPECT_EMPTY_REFERRER));
    title_watcher->AlsoWaitForTitle(
        GetExpectedTitle(url, EXPECT_FULL_REFERRER));
    title_watcher->AlsoWaitForTitle(
        GetExpectedTitle(url, EXPECT_ORIGIN_AS_REFERRER));
  }

  // Returns a string representation of a given |referrer_policy|.
  std::string ReferrerPolicyToString(blink::WebReferrerPolicy referrer_policy) {
    switch (referrer_policy) {
      case blink::WebReferrerPolicyDefault:
        return "default";
      case blink::WebReferrerPolicyOrigin:
        return "origin";
      case blink::WebReferrerPolicyAlways:
        return "always";
      case blink::WebReferrerPolicyNever:
        return "never";
      default:
        NOTREACHED();
        return "";
    }
  }

  // Navigates from a page with a given |referrer_policy| and checks that the
  // reported referrer matches the expectation.
  // Parameters:
  //  referrer_policy:   The referrer policy to test.
  //  start_on_https:    True if the test should start on an HTTPS page.
  //  target_blank:      True if the link that is generated should have the
  //                     attribute target=_blank
  //  redirect:          True if the link target should first do a server
  //                     redirect before evaluating the passed referrer.
  //  opens_new_tab:     True if this test opens a new tab.
  //  button:            If not WebMouseEvent::ButtonNone, click on the
  //                     link with the specified mouse button.
  //  expected_referrer: The kind of referrer to expect.
  //
  // Returns:
  //  The URL of the first page navigated to.
  GURL RunReferrerTest(const blink::WebReferrerPolicy referrer_policy,
                       bool start_on_https,
                       bool target_blank,
                       bool redirect,
                       bool opens_new_tab,
                       blink::WebMouseEvent::Button button,
                       ExpectedReferrer expected_referrer) {
    GURL start_url;
    net::SpawnedTestServer* start_server =
        start_on_https ? ssl_test_server_.get() : test_server_.get();
    start_url = start_server->GetURL(
        std::string("files/referrer-policy-start.html?") +
        "policy=" + ReferrerPolicyToString(referrer_policy) +
        "&port=" + base::IntToString(test_server_->host_port_pair().port()) +
        "&ssl_port=" +
            base::IntToString(ssl_test_server_->host_port_pair().port()) +
        "&redirect=" + (redirect ? "true" : "false") +
        "&link=" +
            (button == blink::WebMouseEvent::ButtonNone ? "false" : "true") +
        "&target=" + (target_blank ? "_blank" : ""));

    ui_test_utils::WindowedTabAddedNotificationObserver tab_added_observer(
        content::NotificationService::AllSources());

    base::string16 expected_title =
        GetExpectedTitle(start_url, expected_referrer);
    content::WebContents* tab =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::TitleWatcher title_watcher(tab, expected_title);

    // Watch for all possible outcomes to avoid timeouts if something breaks.
    AddAllPossibleTitles(start_url, &title_watcher);

    ui_test_utils::NavigateToURL(browser(), start_url);

    if (button != blink::WebMouseEvent::ButtonNone) {
      blink::WebMouseEvent mouse_event;
      mouse_event.type = blink::WebInputEvent::MouseDown;
      mouse_event.button = button;
      mouse_event.x = 15;
      mouse_event.y = 15;
      mouse_event.clickCount = 1;
      tab->GetRenderViewHost()->ForwardMouseEvent(mouse_event);
      mouse_event.type = blink::WebInputEvent::MouseUp;
      tab->GetRenderViewHost()->ForwardMouseEvent(mouse_event);
    }

    if (opens_new_tab) {
      tab_added_observer.Wait();
      tab = tab_added_observer.GetTab();
      EXPECT_TRUE(tab);
      content::WaitForLoadStop(tab);
      EXPECT_EQ(expected_title, tab->GetTitle());
    } else {
      EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
    }

    EXPECT_EQ(referrer_policy,
              tab->GetController().GetActiveEntry()->GetReferrer().policy);

    return start_url;
  }

  scoped_ptr<net::SpawnedTestServer> test_server_;
  scoped_ptr<net::SpawnedTestServer> ssl_test_server_;
};

// The basic behavior of referrer policies is covered by layout tests in
// http/tests/security/referrer-policy-*. These tests cover (hopefully) all
// code paths chrome uses to navigate. To keep the number of combinations down,
// we only test the "origin" policy here.
//
// Some tests are marked as FAILS, see http://crbug.com/124750

// Content initiated navigation, from HTTP to HTTP.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, Origin) {
  RunReferrerTest(blink::WebReferrerPolicyOrigin, false, false, false, false,
                  blink::WebMouseEvent::ButtonNone,
                  EXPECT_ORIGIN_AS_REFERRER);
}

// Content initiated navigation, from HTTPS to HTTP.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, HttpsDefault) {
  RunReferrerTest(blink::WebReferrerPolicyOrigin, true, false, false, false,
                  blink::WebMouseEvent::ButtonNone,
                  EXPECT_ORIGIN_AS_REFERRER);
}

// User initiated navigation, from HTTP to HTTP.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, LeftClickOrigin) {
  RunReferrerTest(blink::WebReferrerPolicyOrigin, false, false, false, false,
                  blink::WebMouseEvent::ButtonLeft,
                  EXPECT_ORIGIN_AS_REFERRER);
}

// User initiated navigation, from HTTPS to HTTP.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, HttpsLeftClickOrigin) {
  RunReferrerTest(blink::WebReferrerPolicyOrigin, true, false, false, false,
                  blink::WebMouseEvent::ButtonLeft,
                  EXPECT_ORIGIN_AS_REFERRER);
}

// User initiated navigation, middle click, from HTTP to HTTP.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, MiddleClickOrigin) {
  RunReferrerTest(blink::WebReferrerPolicyOrigin, false, false, false, true,
                  blink::WebMouseEvent::ButtonMiddle,
                  EXPECT_ORIGIN_AS_REFERRER);
}

// User initiated navigation, middle click, from HTTPS to HTTP.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, HttpsMiddleClickOrigin) {
  RunReferrerTest(blink::WebReferrerPolicyOrigin, true, false, false, true,
                  blink::WebMouseEvent::ButtonMiddle,
                  EXPECT_ORIGIN_AS_REFERRER);
}

// User initiated navigation, target blank, from HTTP to HTTP.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, TargetBlankOrigin) {
  RunReferrerTest(blink::WebReferrerPolicyOrigin, false, true, false, true,
                  blink::WebMouseEvent::ButtonLeft,
                  EXPECT_ORIGIN_AS_REFERRER);
}

// User initiated navigation, target blank, from HTTPS to HTTP.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, HttpsTargetBlankOrigin) {
  RunReferrerTest(blink::WebReferrerPolicyOrigin, true, true, false, true,
                  blink::WebMouseEvent::ButtonLeft,
                  EXPECT_ORIGIN_AS_REFERRER);
}

// User initiated navigation, middle click, target blank, from HTTP to HTTP.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, MiddleClickTargetBlankOrigin) {
  RunReferrerTest(blink::WebReferrerPolicyOrigin, false, true, false, true,
                  blink::WebMouseEvent::ButtonMiddle,
                  EXPECT_ORIGIN_AS_REFERRER);
}

// User initiated navigation, middle click, target blank, from HTTPS to HTTP.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, HttpsMiddleClickTargetBlankOrigin) {
  RunReferrerTest(blink::WebReferrerPolicyOrigin, true, true, false, true,
                  blink::WebMouseEvent::ButtonMiddle,
                  EXPECT_ORIGIN_AS_REFERRER);
}

// Context menu, from HTTP to HTTP.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, MAYBE_ContextMenuOrigin) {
  ContextMenuNotificationObserver context_menu_observer(
      IDC_CONTENT_CONTEXT_OPENLINKNEWTAB);
  RunReferrerTest(blink::WebReferrerPolicyOrigin, false, false, false, true,
                  blink::WebMouseEvent::ButtonRight,
                  EXPECT_ORIGIN_AS_REFERRER);
}

// Context menu, from HTTPS to HTTP.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, MAYBE_HttpsContextMenuOrigin) {
  ContextMenuNotificationObserver context_menu_observer(
      IDC_CONTENT_CONTEXT_OPENLINKNEWTAB);
  RunReferrerTest(blink::WebReferrerPolicyOrigin, true, false, false, true,
                  blink::WebMouseEvent::ButtonRight,
                  EXPECT_ORIGIN_AS_REFERRER);
}

// Content initiated navigation, from HTTP to HTTP via server redirect.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, Redirect) {
  RunReferrerTest(blink::WebReferrerPolicyOrigin, false, false, true, false,
                  blink::WebMouseEvent::ButtonNone,
                  EXPECT_ORIGIN_AS_REFERRER);
}

// Content initiated navigation, from HTTPS to HTTP via server redirect.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, HttpsRedirect) {
  RunReferrerTest(blink::WebReferrerPolicyOrigin, true, false, true, false,
                  blink::WebMouseEvent::ButtonNone,
                  EXPECT_ORIGIN_AS_REFERRER);
}

// User initiated navigation, from HTTP to HTTP via server redirect.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, LeftClickRedirect) {
  RunReferrerTest(blink::WebReferrerPolicyOrigin, false, false, true, false,
                  blink::WebMouseEvent::ButtonLeft,
                  EXPECT_ORIGIN_AS_REFERRER);
}

// User initiated navigation, from HTTPS to HTTP via server redirect.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, HttpsLeftClickRedirect) {
  RunReferrerTest(blink::WebReferrerPolicyOrigin, true, false, true, false,
                  blink::WebMouseEvent::ButtonLeft,
                  EXPECT_ORIGIN_AS_REFERRER);
}

// User initiated navigation, middle click, from HTTP to HTTP via server
// redirect.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, MiddleClickRedirect) {
  RunReferrerTest(blink::WebReferrerPolicyOrigin, false, false, true, true,
                  blink::WebMouseEvent::ButtonMiddle,
                  EXPECT_ORIGIN_AS_REFERRER);
}

// User initiated navigation, middle click, from HTTPS to HTTP via server
// redirect.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, HttpsMiddleClickRedirect) {
  RunReferrerTest(blink::WebReferrerPolicyOrigin, true, false, true, true,
                  blink::WebMouseEvent::ButtonMiddle,
                  EXPECT_ORIGIN_AS_REFERRER);
}

// User initiated navigation, target blank, from HTTP to HTTP via server
// redirect.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, TargetBlankRedirect) {
  RunReferrerTest(blink::WebReferrerPolicyOrigin, false, true, true, true,
                  blink::WebMouseEvent::ButtonLeft,
                  EXPECT_ORIGIN_AS_REFERRER);
}

// User initiated navigation, target blank, from HTTPS to HTTP via server
// redirect.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, HttpsTargetBlankRedirect) {
  RunReferrerTest(blink::WebReferrerPolicyOrigin, true, true, true, true,
                  blink::WebMouseEvent::ButtonLeft,
                  EXPECT_ORIGIN_AS_REFERRER);
}

// User initiated navigation, middle click, target blank, from HTTP to HTTP via
// server redirect.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, MiddleClickTargetBlankRedirect) {
  RunReferrerTest(blink::WebReferrerPolicyOrigin, false, true, true, true,
                  blink::WebMouseEvent::ButtonMiddle,
                  EXPECT_ORIGIN_AS_REFERRER);
}

// User initiated navigation, middle click, target blank, from HTTPS to HTTP
// via server redirect.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest,
                       HttpsMiddleClickTargetBlankRedirect) {
  RunReferrerTest(blink::WebReferrerPolicyOrigin, true, true, true, true,
                  blink::WebMouseEvent::ButtonMiddle,
                  EXPECT_ORIGIN_AS_REFERRER);
}

// Context menu, from HTTP to HTTP via server redirect.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, MAYBE_ContextMenuRedirect) {
  ContextMenuNotificationObserver context_menu_observer(
      IDC_CONTENT_CONTEXT_OPENLINKNEWTAB);
  RunReferrerTest(blink::WebReferrerPolicyOrigin, false, false, true, true,
                  blink::WebMouseEvent::ButtonRight,
                  EXPECT_ORIGIN_AS_REFERRER);
}

// Context menu, from HTTPS to HTTP via server redirect.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, MAYBE_HttpsContextMenuRedirect) {
  ContextMenuNotificationObserver context_menu_observer(
      IDC_CONTENT_CONTEXT_OPENLINKNEWTAB);
  RunReferrerTest(blink::WebReferrerPolicyOrigin, true, false, true, true,
                  blink::WebMouseEvent::ButtonRight,
                  EXPECT_ORIGIN_AS_REFERRER);
}

// Tests history navigation actions: Navigate from A to B with a referrer
// policy, then navigate to C, back to B, and reload.
IN_PROC_BROWSER_TEST_F(ReferrerPolicyTest, History) {
  // Navigate from A to B.
  GURL start_url = RunReferrerTest(blink::WebReferrerPolicyOrigin,
                                   true,
                                   false,
                                   true,
                                   false,
                                   blink::WebMouseEvent::ButtonLeft,
                                   EXPECT_ORIGIN_AS_REFERRER);

  // Navigate to C.
  ui_test_utils::NavigateToURL(browser(), test_server_->GetURL(std::string()));

  base::string16 expected_title =
      GetExpectedTitle(start_url, EXPECT_ORIGIN_AS_REFERRER);
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  scoped_ptr<content::TitleWatcher> title_watcher(
      new content::TitleWatcher(tab, expected_title));

  // Watch for all possible outcomes to avoid timeouts if something breaks.
  AddAllPossibleTitles(start_url, title_watcher.get());

  // Go back to B.
  chrome::GoBack(browser(), CURRENT_TAB);
  EXPECT_EQ(expected_title, title_watcher->WaitAndGetTitle());

  title_watcher.reset(new content::TitleWatcher(tab, expected_title));
  AddAllPossibleTitles(start_url, title_watcher.get());

  // Reload to B.
  chrome::Reload(browser(), CURRENT_TAB);
  EXPECT_EQ(expected_title, title_watcher->WaitAndGetTitle());

  title_watcher.reset(new content::TitleWatcher(tab, expected_title));
  AddAllPossibleTitles(start_url, title_watcher.get());

  // Shift-reload to B.
  chrome::ReloadIgnoringCache(browser(), CURRENT_TAB);
  EXPECT_EQ(expected_title, title_watcher->WaitAndGetTitle());
}
