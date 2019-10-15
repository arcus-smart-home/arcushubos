/*
 * Copyright 2019 Arcus Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdint.h>

#define HEADER_VERSION_0   0

// The firmware header include checksum (SHA256) and signature data
//  as well as target platform info, version, etc.

#define FW_VER_LEN        24
#define FW_MODEL_LEN      16
#define FW_CUSTOMER_LEN   16
#define FW_CKSUM_LEN      64
#define FW_KEY_LEN        16
#define FW_IV_LEN         16
#define SIGNED_HDR_LEN    512

typedef struct fw_header {

  uint16_t        header_version;
  uint16_t        reserved;
  uint32_t        image_size;
  char            fw_version[FW_VER_LEN];
  char            fw_model[FW_MODEL_LEN];
  char            fw_customer[FW_CUSTOMER_LEN];
  /* Allow for trailing null and word align these strings */
  char            fw_cksum[FW_CKSUM_LEN + 2];
  char            fw_key[(FW_KEY_LEN * 2) + 2];
  char            fw_iv[(FW_IV_LEN * 2) + 2];

} fw_header_t;
