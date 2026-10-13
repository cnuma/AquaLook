#include "UpdateSignature.h"

#include <cstring>
#include <mbedtls/base64.h>
#include <mbedtls/pk.h>
#include <mbedtls/sha256.h>

#include "UpdateSignatureKeys.h"

namespace UpdateSignature {

namespace {
constexpr const char* FORMAT = "aqualook-sig-v1";
constexpr size_t ID_LEN = 16U;
// Signature ECDSA P-256 encodee DER : 72 octets au plus.
constexpr size_t DER_SIG_MAX = 80U;

bool isSpace(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

// Decoupe "format id base64" sans allocation. Rend false si la ligne n'a pas
// exactement trois mots.
bool splitWords(const char* text, size_t len, const char* word[3], size_t wordLen[3]) {
    size_t n = 0U, i = 0U;
    while (i < len) {
        while (i < len && isSpace(text[i])) ++i;
        if (i >= len) break;
        if (n == 3U) return false;
        const size_t start = i;
        while (i < len && !isSpace(text[i]) && text[i] != '\0') ++i;
        word[n] = text + start;
        wordLen[n] = i - start;
        ++n;
        if (i < len && text[i] == '\0') break;
    }
    return n == 3U;
}
}  // namespace

Result verify(const uint8_t* data, size_t dataLen,
              const char* sigText, size_t sigLen,
              char* keyIdOut, size_t keyIdOutSize) {
    if (keyIdOut && keyIdOutSize) keyIdOut[0] = '\0';
    if (!data || !sigText || sigLen == 0U || sigLen > SIG_FILE_MAX_BYTES) return Result::UNREADABLE;

    const char* word[3] = {nullptr, nullptr, nullptr};
    size_t wordLen[3] = {0U, 0U, 0U};
    if (!splitWords(sigText, sigLen, word, wordLen)) return Result::UNREADABLE;
    if (wordLen[0] != strlen(FORMAT) || strncmp(word[0], FORMAT, wordLen[0]) != 0) return Result::UNREADABLE;
    if (wordLen[1] != ID_LEN) return Result::UNREADABLE;
    if (keyIdOut && keyIdOutSize > ID_LEN) {
        memcpy(keyIdOut, word[1], ID_LEN);
        keyIdOut[ID_LEN] = '\0';
    }

    const UpdateSigningKey* key = nullptr;
    for (size_t k = 0U; k < UPDATE_SIGNING_KEY_COUNT; ++k) {
        if (strncmp(UPDATE_SIGNING_KEYS[k].id, word[1], ID_LEN) == 0) { key = &UPDATE_SIGNING_KEYS[k]; break; }
    }
    if (!key) return Result::UNKNOWN_KEY;

    unsigned char der[DER_SIG_MAX];
    size_t derLen = 0U;
    if (mbedtls_base64_decode(der, sizeof(der), &derLen,
                              reinterpret_cast<const unsigned char*>(word[2]), wordLen[2]) != 0 ||
        derLen == 0U) {
        return Result::UNREADABLE;
    }

    unsigned char hash[32];
    if (mbedtls_sha256_ret(data, dataLen, hash, 0) != 0) return Result::INTERNAL;

    mbedtls_pk_context pk;
    mbedtls_pk_init(&pk);
    Result r = Result::INTERNAL;
    if (mbedtls_pk_parse_public_key(&pk, key->der, key->len) == 0 &&
        mbedtls_pk_can_do(&pk, MBEDTLS_PK_ECKEY)) {
        r = (mbedtls_pk_verify(&pk, MBEDTLS_MD_SHA256, hash, sizeof(hash), der, derLen) == 0)
                ? Result::OK : Result::INVALID;
    }
    mbedtls_pk_free(&pk);
    return r;
}

const char* code(Result r) {
    switch (r) {
        case Result::OK:          return "signature-ok";
        case Result::UNREADABLE:  return "signature-illisible";
        case Result::UNKNOWN_KEY: return "signature-cle-inconnue";
        case Result::INVALID:     return "signature-invalide";
        default:                  return "signature-erreur-interne";
    }
}

}  // namespace UpdateSignature
