/*
 * Copyright 2019-present MongoDB, Inc.
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

#include "../mongocrypt-crypto-private.h"
#include "../mongocrypt-private.h"

#ifdef MONGOCRYPT_ENABLE_CRYPTO_COMMON_CRYPTO

#include <CommonCrypto/CommonCryptor.h>
#include <CommonCrypto/CommonHMAC.h>
#include <CommonCrypto/CommonRandom.h>


bool _native_crypto_initialized = false;

void
_native_crypto_init ()
{
   _native_crypto_initialized = true;
}


bool
_native_crypto_aes_256_cbc_encrypt_with_mode (aes_256_args_t args, CCMode mode)
{
   bool ret = false;
   CCCryptorRef ctx = NULL;
   CCCryptorStatus cc_status;
   size_t intermediate_bytes_written;
   mongocrypt_status_t *status = args.status;

   cc_status = CCCryptorCreateWithMode (kCCEncrypt,
                                        mode,
                                        kCCAlgorithmAES,
                                        0 /* defaults to CBC w/ no padding */,
                                        args.iv->data,
                                        args.key->data,
                                        kCCKeySizeAES256,
                                        NULL,
                                        0,
                                        0,
                                        0,
                                        &ctx);

   if (cc_status != kCCSuccess) {
      CLIENT_ERR ("error initializing cipher: %d", (int) cc_status);
      goto done;
   }

   *args.bytes_written = 0;

   cc_status = CCCryptorUpdate (ctx,
                                args.in->data,
                                args.in->len,
                                args.out->data,
                                args.out->len,
                                &intermediate_bytes_written);
   if (cc_status != kCCSuccess) {
      CLIENT_ERR ("error encrypting: %d", (int) cc_status);
      goto done;
   }
   *args.bytes_written = intermediate_bytes_written;


   cc_status = CCCryptorFinal (ctx,
                               args.out->data + *args.bytes_written,
                               args.out->len - *args.bytes_written,
                               &intermediate_bytes_written);
   *args.bytes_written += intermediate_bytes_written;

   if (cc_status != kCCSuccess) {
      CLIENT_ERR ("error finalizing: %d", (int) cc_status);
      goto done;
   }

   ret = true;
done:
   CCCryptorRelease (ctx);
   return ret;
}

bool
_native_crypto_aes_256_cbc_encrypt (aes_256_args_t args)
{
   return _native_crypto_aes_256_cbc_encrypt_with_mode (args, kCCModeCBC);
}

bool
_native_crypto_aes_256_ctr_encrypt (aes_256_args_t args)
{
   return _native_crypto_aes_256_cbc_encrypt_with_mode (args, kCCModeCTR);
}

/* Note, the decrypt function is almost exactly the same as the encrypt
 * functions except for the kCCDecrypt and the error message. */
bool
_native_crypto_aes_256_cbc_decrypt_with_mode (aes_256_args_t args, CCMode mode)
{
   bool ret = false;
   CCCryptorRef ctx = NULL;
   CCCryptorStatus cc_status;
   size_t intermediate_bytes_written;
   mongocrypt_status_t *status = args.status;

   cc_status = CCCryptorCreateWithMode (kCCDecrypt,
                                        mode,
                                        kCCAlgorithmAES,
                                        0 /* defaults to CBC w/ no padding */,
                                        args.iv->data,
                                        args.key->data,
                                        kCCKeySizeAES256,
                                        NULL,
                                        0,
                                        0,
                                        0,
                                        &ctx);

   if (cc_status != kCCSuccess) {
      CLIENT_ERR ("error initializing cipher: %d", (int) cc_status);
      goto done;
   }

   *args.bytes_written = 0;
   cc_status = CCCryptorUpdate (ctx,
                                args.in->data,
                                args.in->len,
                                args.out->data,
                                args.out->len,
                                &intermediate_bytes_written);
   if (cc_status != kCCSuccess) {
      CLIENT_ERR ("error decrypting: %d", (int) cc_status);
      goto done;
   }
   *args.bytes_written = intermediate_bytes_written;

   cc_status = CCCryptorFinal (ctx,
                               args.out->data + *args.bytes_written,
                               args.out->len - *args.bytes_written,
                               &intermediate_bytes_written);
   *args.bytes_written += intermediate_bytes_written;

   if (cc_status != kCCSuccess) {
      CLIENT_ERR ("error finalizing: %d", (int) cc_status);
      goto done;
   }

   ret = true;
done:
   CCCryptorRelease (ctx);
   return ret;
}

bool
_native_crypto_aes_256_cbc_decrypt (aes_256_args_t args)
{
   return _native_crypto_aes_256_cbc_decrypt_with_mode (args, kCCModeCBC);
}

bool
_native_crypto_aes_256_ctr_decrypt (aes_256_args_t args)
{
   return _native_crypto_aes_256_cbc_decrypt_with_mode (args, kCCModeCTR);
}


/* _hmac_with_algorithm computes an HMAC of @in with the algorithm specified by
 * @algorithm.
 * @key is the input key.
 * @out is the output. @out must be allocated by the caller with
 * the expected length @expect_out_len for the output.
 * Returns false and sets @status on error. @status is required. */
bool
_hmac_with_algorithm (CCHmacAlgorithm algorithm,
                      const _mongocrypt_buffer_t *key,
                      const _mongocrypt_buffer_t *in,
                      _mongocrypt_buffer_t *out,
                      uint32_t expect_out_len,
                      mongocrypt_status_t *status)
{
   CCHmacContext *ctx;

   if (out->len != expect_out_len) {
      CLIENT_ERR ("out does not contain %" PRIu32 " bytes", expect_out_len);
      return false;
   }

   ctx = bson_malloc0 (sizeof (*ctx));
   BSON_ASSERT (ctx);


   CCHmacInit (ctx, algorithm, key->data, key->len);
   CCHmacUpdate (ctx, in->data, in->len);
   CCHmacFinal (ctx, out->data);
   bson_free (ctx);
   return true;
}

bool
_native_crypto_hmac_sha_512 (const _mongocrypt_buffer_t *key,
                             const _mongocrypt_buffer_t *in,
                             _mongocrypt_buffer_t *out,
                             mongocrypt_status_t *status)
{
   return _hmac_with_algorithm (
      kCCHmacAlgSHA512, key, in, out, MONGOCRYPT_HMAC_SHA512_LEN, status);
}


bool
_native_crypto_random (_mongocrypt_buffer_t *out,
                       uint32_t count,
                       mongocrypt_status_t *status)
{
   CCRNGStatus ret = CCRandomGenerateBytes (out->data, (size_t) count);
   if (ret != kCCSuccess) {
      CLIENT_ERR ("failed to generate random iv: %d", (int) ret);
      return false;
   }
   return true;
}

bool
_native_crypto_hmac_sha_256 (const _mongocrypt_buffer_t *key,
                             const _mongocrypt_buffer_t *in,
                             _mongocrypt_buffer_t *out,
                             mongocrypt_status_t *status)
{
   return _hmac_with_algorithm (
      kCCHmacAlgSHA256, key, in, out, MONGOCRYPT_HMAC_SHA256_LEN, status);
}

#endif /* MONGOCRYPT_ENABLE_CRYPTO_COMMON_CRYPTO */
