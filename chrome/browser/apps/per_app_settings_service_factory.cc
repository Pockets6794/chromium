// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/per_app_settings_service_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/apps/per_app_settings_service.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"

// static
PerAppSettingsServiceFactory* PerAppSettingsServiceFactory::GetInstance() {
  return Singleton<PerAppSettingsServiceFactory>::get();
}

// static
PerAppSettingsService* PerAppSettingsServiceFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<PerAppSettingsService*>(
      GetInstance()->GetServiceForBrowserContext(
          browser_context, true /* create */));
}

PerAppSettingsServiceFactory::PerAppSettingsServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "PerAppSettingsServiceFactory",
          BrowserContextDependencyManager::GetInstance()) {
}

PerAppSettingsServiceFactory::~PerAppSettingsServiceFactory() {}

BrowserContextKeyedService*
PerAppSettingsServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new PerAppSettingsService;
}

bool PerAppSettingsServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return false;
}

content::BrowserContext* PerAppSettingsServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}
