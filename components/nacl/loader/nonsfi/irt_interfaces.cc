// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/loader/nonsfi/irt_interfaces.h"

#include <cstring>

namespace nacl {
namespace nonsfi {

namespace {

// This table keeps a pair of IRT entry (such as nacl_irt_basic, nacl_irt_fdio
// etc.) and its registered name with its version (such as NACL_IRT_BASIC_v0_1,
// NACL_IRT_FDIO_v0_1, etc.).
struct NaClInterfaceTable {
  const char* name;
  const void* table;
  size_t size;
};

#define NACL_INTERFACE_TABLE(name, value) { name, &value, sizeof(value) }
const NaClInterfaceTable kIrtInterfaces[] = {
  NACL_INTERFACE_TABLE(NACL_IRT_BASIC_v0_1, kIrtBasic),
};
#undef NACL_INTERFACE_TABLE

}  // namespace

size_t NaClIrtInterface(const char* interface_ident,
                        void* table, size_t tablesize) {
  for (size_t i = 0; i < arraysize(kIrtInterfaces); ++i) {
    if (std::strcmp(interface_ident, kIrtInterfaces[i].name) == 0) {
      const size_t size = kIrtInterfaces[i].size;
      if (size <= tablesize) {
        std::memcpy(table, kIrtInterfaces[i].table, size);
        return size;
      }
      break;
    }
  }
  return 0;
}

}  // namespace nonsfi
}  // namespace nacl
