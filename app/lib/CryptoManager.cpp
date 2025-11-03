#include "CryptoManager.hpp"
#include "Utils.hpp"
#include <cstring>
#include <openssl/types.h>
#include <string.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <iostream>


CryptoManager::CryptoManager(const std::string& env_pc, const std::string& env_rr)
    : env_pc(env_pc), env_rr(env_rr) {}

std::string CryptoManager::reconstruct()
{
    std::string kp_1 = deobfuscate(env_pc);
    std::string kp_2 = deobfuscate(std::string(CryptoManager::embedded_pc));
    std::string recomposed_key = reassemble_key(kp_1, kp_2);
    std::string k = aes256_decrypt(Utils::hex_to_vector(env_rr), recomposed_key);

    return k;
}

std::string CryptoManager::reassemble_key(const std::string& part1, const std::string& part2)
{
    return part2 + part1;
}


std::string CryptoManager::base64_decode(const std::string& encoded) {
    BIO *bio, *b64;
    int decode_len = encoded.size() * 3 / 4; // Approximation of the decoded size
    char *buffer = new char[decode_len + 1]; // +1 for the null character
    memset(buffer, 0, decode_len + 1);

    b64 = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL); // Ignore newline characters
    bio = BIO_new_mem_buf(encoded.c_str(), -1); // Buffer creation
    bio = BIO_push(b64, bio);

    int decoded_length = BIO_read(bio, buffer, decode_len);
    if (decoded_length < 0) {
        delete[] buffer;
        BIO_free_all(bio);
        throw std::runtime_error("Erreur dans le décodage Base64.");
    }

    BIO_free_all(bio);

    std::string decoded(buffer, decoded_length);
    delete[] buffer;
    return decoded;
}


std::string CryptoManager::deobfuscate_with_salt(const std::string& obfuscated_data,
                                                 const std::string& salt)
{
    std::string decoded_data;
    size_t salt_index = 0;

    for (char c : obfuscated_data) {
        char original_char = c ^ salt[salt_index % salt.length()];
        salt_index++;

        decoded_data += original_char;
    }

    return decoded_data;
}


std::string CryptoManager::deobfuscate(const std::string& obfuscated_data) {
    constexpr size_t salt_length = 16;

    if (obfuscated_data.length() <= salt_length) {
        throw std::runtime_error("Invalid data: Salt or obfuscated data missing.");
    }

    std::string salt = obfuscated_data.substr(0, salt_length);
    std::string base64_encoded = obfuscated_data.substr(salt_length);
    std::string obfuscated_data_with_salt = base64_decode(base64_encoded);

    return deobfuscate_with_salt(obfuscated_data_with_salt, salt);
}


std::string CryptoManager::aes256_decrypt(const std::vector<unsigned char>& ciphertext, const std::string& key) {
    if (key.size() != 32) {
        throw std::runtime_error("Key must be 32 bytes (256 bits) for AES-256 decryption.");
    }

    // Extract the IV (first 16 bytes of the ciphertext)
    if (ciphertext.size() < 16) {
        throw std::runtime_error("Ciphertext is too short to contain an IV.");
    }
    unsigned char iv[16];
    std::memcpy(iv, ciphertext.data(), sizeof(iv));

    // Output buffer
    std::vector<unsigned char> plaintext(ciphertext.size());
    int len = 0, plaintext_len = 0;

    // Create and initialize the context
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        throw std::runtime_error("Failed to create EVP_CIPHER_CTX.");
    }

    try {
        if (EVP_DecryptInit_ex(ctx,
                               EVP_aes_256_cbc(),
                               nullptr,
                               reinterpret_cast<const unsigned char*>(key.data()),
                               iv) != 1) {
            throw std::runtime_error("Decryption initialization failed.");
        }

        if (EVP_DecryptUpdate(ctx, plaintext.data(), &len, ciphertext.data() + sizeof(iv), ciphertext.size() - sizeof(iv)) != 1) {
            throw std::runtime_error("Decryption failed.");
        }
        plaintext_len += len;

        if (EVP_DecryptFinal_ex(ctx, plaintext.data() + len, &len) != 1) {
            throw std::runtime_error("Final decryption step failed.");
        }
        plaintext_len += len;
        plaintext.resize(plaintext_len);
    } catch (...) {
        EVP_CIPHER_CTX_free(ctx);
        throw;
    }

    EVP_CIPHER_CTX_free(ctx);
    return std::string(plaintext.begin(), plaintext.end());
}
