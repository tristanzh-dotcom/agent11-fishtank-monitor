#pragma once
#include <CommonCrypto/CommonHMAC.h>
#include <cstddef>
#include <cstdint>
// Use real HMAC-SHA256 on the host in place of the MCU crypto backend.
constexpr int MBEDTLS_MD_SHA256 = 1;
struct mbedtls_md_info_t {};
struct mbedtls_md_context_t { CCHmacContext context; };
inline void mbedtls_md_init(mbedtls_md_context_t*) {}
inline const mbedtls_md_info_t* mbedtls_md_info_from_type(int) {
  static mbedtls_md_info_t info;
  return &info;
}
inline int mbedtls_md_setup(mbedtls_md_context_t*, const mbedtls_md_info_t*, int) { return 0; }
inline int mbedtls_md_hmac_starts(mbedtls_md_context_t* c, const std::uint8_t* key, std::size_t n) {
  CCHmacInit(&c->context, kCCHmacAlgSHA256, key, n); return 0;
}
inline int mbedtls_md_hmac_update(mbedtls_md_context_t* c, const std::uint8_t* data, std::size_t n) {
  CCHmacUpdate(&c->context, data, n); return 0;
}
inline int mbedtls_md_hmac_finish(mbedtls_md_context_t* c, std::uint8_t* out) {
  CCHmacFinal(&c->context, out); return 0;
}
inline void mbedtls_md_free(mbedtls_md_context_t*) {}
