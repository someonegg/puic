// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/openssl_util.h"

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <openssl/crypto.h>
#include <openssl/err.h>

namespace crypto {

void EnsureOpenSSLInit() {
  // CRYPTO_library_init may be safely called concurrently.
  CRYPTO_library_init();
}

void ClearOpenSSLERRStack(const base::Location& location) {
  ERR_clear_error();
}

}  // namespace crypto
