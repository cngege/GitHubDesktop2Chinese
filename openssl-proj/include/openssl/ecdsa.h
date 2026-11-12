/*
 * Copyright 2002-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/* OpenSSL 3.x 中 ecdsa.h 与 ecdh.h 均为仅包含 <openssl/ec.h> 的兼容头，
 * 两者原本内容完全相同。此处统一转发到 ecdh.h 以消除重复维护，
 * 同时保留本文件路径，避免依赖 <openssl/ecdsa.h> 的代码失效。 */
#include <openssl/ecdh.h>
