#include "crypto.hpp"
#include <iomanip>
#include <openssl/evp.h>
#include <sstream>

std::string Crypto::ComputeSHA256(const std::vector<char>& bytes) {
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_sha256(), NULL);
    EVP_DigestUpdate(ctx, bytes.data(), bytes.size());

    unsigned char result[EVP_MAX_MD_SIZE];
    unsigned int hash_len = 0;
    EVP_DigestFinal_ex(ctx, result, &hash_len);
    EVP_MD_CTX_free(ctx);

    std::stringstream s;
    for (unsigned int i = 0; i < hash_len; i++) {
        s << std::hex << std::setw(2) << std::setfill('0') << (int)result[i];
    }
    return s.str();
}