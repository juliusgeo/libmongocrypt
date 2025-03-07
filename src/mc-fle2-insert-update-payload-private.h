/*
 * Copyright 2022-present MongoDB, Inc.
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

#ifndef MC_FLE2_INSERT_UPDATE_PAYLOAD_PRIVATE_H
#define MC_FLE2_INSERT_UPDATE_PAYLOAD_PRIVATE_H

#include <bson.h>

#include "mongocrypt.h"
#include "mongocrypt-private.h"
#include "mongocrypt-buffer-private.h"

/**
 * FLE2InsertUpdatePayload represents an FLE2 payload of an indexed field to
 * insert or update. It is created client side.
 *
 * FLE2InsertUpdatePayload has the following data layout:
 *
 * struct {
 *   uint8_t fle_blob_subtype = 4;
 *   uint8_t bson[16];
 * } FLE2InsertUpdatePayload;
 *
 * bson is a BSON document of this form:
 * d: <binary> // EDCDerivedFromDataTokenAndCounter
 * s: <binary> // ESCDerivedFromDataTokenAndCounter
 * c: <binary> // ECCDerivedFromDataTokenAndCounter
 * p: <binary> // Encrypted Tokens
 * u: <UUID>   // Index KeyId
 * t: <int32>  // Encrypted type
 * v: <binary> // Encrypted value
 * e: <binary> // ServerDataEncryptionLevel1Token
 *
 * p is the result of:
 * Encrypt(
 * key=ECOCToken,
 * plaintext=(
 *    ESCDerivedFromDataTokenAndCounter ||
 *    ECCDerivedFromDataTokenAndCounter)
 * )
 *
 * v is the result of:
 * UserKeyId || EncryptAEAD(
 *    key=UserKey,
 *    plaintext=value
 *    associated_data=UserKeyId)
 */

typedef struct {
   _mongocrypt_buffer_t edcDerivedToken;       // d
   _mongocrypt_buffer_t escDerivedToken;       // s
   _mongocrypt_buffer_t eccDerivedToken;       // c
   _mongocrypt_buffer_t encryptedTokens;       // p
   _mongocrypt_buffer_t indexKeyId;            // u
   bson_type_t valueType;                      // t
   _mongocrypt_buffer_t value;                 // v
   _mongocrypt_buffer_t serverEncryptionToken; // e
   _mongocrypt_buffer_t plaintext;
   _mongocrypt_buffer_t userKeyId;
} mc_FLE2InsertUpdatePayload_t;

void
mc_FLE2InsertUpdatePayload_init (mc_FLE2InsertUpdatePayload_t *payload);

bool
mc_FLE2InsertUpdatePayload_parse (mc_FLE2InsertUpdatePayload_t *out,
                                  const _mongocrypt_buffer_t *in,
                                  mongocrypt_status_t *status);

/* mc_FLE2InsertUpdatePayload_decrypt decrypts ciphertext.
 * Returns NULL and sets @status on error. It is an error to call before
 * mc_FLE2InsertUpdatePayload_parse. */
const _mongocrypt_buffer_t *
mc_FLE2InsertUpdatePayload_decrypt (_mongocrypt_crypto_t *crypto,
                                    mc_FLE2InsertUpdatePayload_t *iup,
                                    const _mongocrypt_buffer_t *user_key,
                                    mongocrypt_status_t *status);

bool
mc_FLE2InsertUpdatePayload_serialize (
   bson_t *out, const mc_FLE2InsertUpdatePayload_t *payload);

void
mc_FLE2InsertUpdatePayload_cleanup (mc_FLE2InsertUpdatePayload_t *payload);

#endif /* MC_FLE2_INSERT_UPDATE_PAYLOAD_PRIVATE_H */
