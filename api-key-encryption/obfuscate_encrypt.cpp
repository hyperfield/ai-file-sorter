#include <utility>
#include <stddef.h>
#include <string>
#include <cstring>

#ifdef _WIN32
#include <cstdlib>

// Need setenv() before including dotenv.h on Windows
int setenv(const char *name, const char *value, int overwrite) {
    if (!overwrite && getenv(name)) return 0;
    return _putenv_s(name, value);
}
#endif

#include "../app/include/external/dotenv.h"
#include <stdexcept>
#include <iostream>
#include <random>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>
#include <openssl/rand.h>


std::string base64_encode(const std::string& input) {
    BIO *bio, *b64;
    BUF_MEM *buffer_ptr;

    b64 = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL); // Ignore newlines
    bio = BIO_new(BIO_s_mem());
    bio = BIO_push(b64, bio);

    BIO_write(bio, input.c_str(), input.size());
    BIO_flush(bio);
    BIO_get_mem_ptr(bio, &buffer_ptr);

    std::string encoded(buffer_ptr->data, buffer_ptr->length);
    BIO_free_all(bio);

    return encoded;
}


std::string base64_decode(const std::string& encoded) {
    BIO *bio, *b64;
    int decode_len = encoded.size() * 3 / 4; // Approximation of the decoded size
    char *buffer = new char[decode_len + 1]; // +1 for the null character
    memset(buffer, 0, decode_len + 1);

    b64 = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL); // Ignore newlines
    bio = BIO_new_mem_buf(encoded.c_str(), -1); // Creation of buffer
    bio = BIO_push(b64, bio);

    int decoded_length = BIO_read(bio, buffer, decode_len);
    if (decoded_length < 0) {
        delete[] buffer;
        BIO_free_all(bio);
        throw std::runtime_error("Could not decode Base64.");
    }

    BIO_free_all(bio);

    std::string decoded(buffer, decoded_length);
    delete[] buffer;
    return decoded;
}


std::pair<std::string, std::string> decompose_key(const std::string& api_key)
{
    // Ensure the key is long enough to split
    if (api_key.length() < 2) {
        throw std::invalid_argument("API key is too short to split.");
    }

    // Split the key into two halves
    size_t mid = api_key.length() / 2;
    std::string part1 = api_key.substr(0, mid);
    std::string part2 = api_key.substr(mid);

    return std::make_pair(part1, part2);
}


std::string reassemble_key(const std::string& part1, const std::string& part2)
{
    return part1 + part2;
}


std::string generate_random_salt(size_t length = 16) {
    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    const size_t max_index = sizeof(charset) - 2;

    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_int_distribution<size_t> distribution(0, max_index);

    std::string salt;
    salt.reserve(length);
    for (size_t i = 0; i < length; ++i) {
        salt += charset[distribution(generator)];
    }
    return salt;
}


std::string obfuscate_with_salt(const std::string& data, const std::string& salt) {
    std::string obfuscated_data;
    size_t salt_index = 0;
    
    for (char c : data) {
        obfuscated_data += c ^ salt[salt_index % salt.length()];
        salt_index++;
    }

    return obfuscated_data;
}


std::string deobfuscate_with_salt(const std::string& obfuscated_data, const std::string& salt) {
    std::string decoded_data;
    size_t salt_index = 0;

    // Iterate over each character in the obfuscated data
    for (char c : obfuscated_data) {
        // XOR with the corresponding salt character
        char original_char = c ^ salt[salt_index % salt.length()];
        salt_index++;

        decoded_data += original_char;
    }

    return decoded_data;
}


std::string deobfuscate(const std::string& obfuscated_data) {
    constexpr size_t salt_length = 16;

    if (obfuscated_data.length() <= salt_length) {
        throw std::runtime_error("Invalid data: Salt or obfuscated data missing.");
    }

    std::string salt = obfuscated_data.substr(0, salt_length);
    std::string base64_encoded = obfuscated_data.substr(salt_length);
    std::string obfuscated_data_with_salt = base64_decode(base64_encoded);

    return deobfuscate_with_salt(obfuscated_data_with_salt, salt);
}


std::string obfuscate(const std::string& data, std::string& salt) {
    std::cout << "Salt: " << salt << std::endl;

    // Obfuscate data with the salt
    std::string obfuscated_data = obfuscate_with_salt(data, salt);

    // Encode obfuscated data with Base64
    std::string base64_encoded = base64_encode(obfuscated_data);

    return salt + base64_encoded;
}


std::vector<unsigned char> aes256_encrypt(const std::string& plaintext, const std::string& key) {
    // Ensure the key length is 32 bytes (256 bits)
    if (key.size() != 32) {
        throw std::runtime_error("Key must be 32 bytes (256 bits) for AES-256 encryption.");
    }

    // Initialization vector (16 bytes for AES-CBC)
    unsigned char iv[16];
    if (!RAND_bytes(iv, sizeof(iv))) {
        throw std::runtime_error("Failed to generate random IV.");
    }

    // Output buffer
    std::vector<unsigned char> ciphertext(plaintext.size() + EVP_MAX_BLOCK_LENGTH);
    int len = 0, ciphertext_len = 0;

    // Create and initialize the context
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        throw std::runtime_error("Failed to create EVP_CIPHER_CTX.");
    }

    try {
        // Initialize encryption operation
        if (EVP_EncryptInit_ex(ctx,
                               EVP_aes_256_cbc(),
                               nullptr,
                               reinterpret_cast<const unsigned char*>(key.data()),
                               iv) != 1) {
            throw std::runtime_error("Encryption initialization failed.");
        }

        // Encrypt the plaintext
        if (EVP_EncryptUpdate(ctx,
                              ciphertext.data(),
                              &len,
                              reinterpret_cast<const unsigned char*>(plaintext.data()),
                              static_cast<int>(plaintext.size())) != 1) {
            throw std::runtime_error("Encryption failed.");
        }
        ciphertext_len += len;

        // Finalize encryption
        if (EVP_EncryptFinal_ex(ctx, ciphertext.data() + len, &len) != 1) {
            throw std::runtime_error("Final encryption step failed.");
        }
        ciphertext_len += len;

        // Add the IV to the start of the ciphertext
        ciphertext.insert(ciphertext.begin(), iv, iv + sizeof(iv));
        ciphertext.resize(ciphertext_len + sizeof(iv)); // Resize to actual ciphertext length
    } catch (...) {
        EVP_CIPHER_CTX_free(ctx);
        throw;
    }

    EVP_CIPHER_CTX_free(ctx);
    return ciphertext;
}


std::string aes256_decrypt(const std::vector<unsigned char>& ciphertext, const std::string& key) {
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
        // Initialize decryption operation
        if (EVP_DecryptInit_ex(ctx,
                               EVP_aes_256_cbc(),
                               nullptr,
                               reinterpret_cast<const unsigned char*>(key.data()),
                               iv) != 1) {
            throw std::runtime_error("Decryption initialization failed.");
        }

        // Decrypt the ciphertext
        if (EVP_DecryptUpdate(ctx, plaintext.data(), &len, ciphertext.data() + sizeof(iv), ciphertext.size() - sizeof(iv)) != 1) {
            throw std::runtime_error("Decryption failed.");
        }
        plaintext_len += len;

        // Finalize decryption
        if (EVP_DecryptFinal_ex(ctx, plaintext.data() + len, &len) != 1) {
            throw std::runtime_error("Final decryption step failed.");
        }
        plaintext_len += len;
        plaintext.resize(plaintext_len); // Resize to actual plaintext length
    } catch (...) {
        EVP_CIPHER_CTX_free(ctx);
        throw;
    }

    EVP_CIPHER_CTX_free(ctx);
    return std::string(plaintext.begin(), plaintext.end());
}


int main()
{
    dotenv::init("encryption.ini");

    std::string api_key = std::getenv("LLM_API_KEY");
    std::string secret_key = std::getenv("SECRET_KEY");

    std::pair<std::string, std::string> decomposed_key = decompose_key(secret_key);
    
    std::string salt1 = generate_random_salt();
    std::string salt2 = generate_random_salt();

    std::string obfuscated_key_part_1 = obfuscate(decomposed_key.first, salt1);
    std::string obfuscated_key_part_2 = obfuscate(decomposed_key.second, salt2);

    std::cout << "Obfuscated Key part 1: " << obfuscated_key_part_1 << std::endl;
    std::cout << "Obfuscated Key part 2: " << obfuscated_key_part_2 << std::endl;

    std::string deobfuscated_key_part_1 = deobfuscate(obfuscated_key_part_1);
    std::string deobfuscated_key_part_2 = deobfuscate(obfuscated_key_part_2);

    std::cout << "Deobfuscated Key part 1: " << deobfuscated_key_part_1 << std::endl;
    std::cout << "Deobfuscated Key part 2: " << deobfuscated_key_part_2 << std::endl;

    std::string recomposed_key = reassemble_key(deobfuscated_key_part_1, deobfuscated_key_part_2);

    if (secret_key == recomposed_key) {
        std::cout << "Recomposed key matches the secret key!" << std::endl;
    } else {
        std::cout << "Recomposed key does NOT match the secret key!!" << std::endl;
    }

    std::vector<unsigned char> encrypted_data = aes256_encrypt(api_key, secret_key);
    std::cout << "Encrypted data (hex): ";
    for (unsigned char c : encrypted_data) {
        printf("%02x", c);
    }
    std::cout << std::endl;

    std::string decrypted_data = aes256_decrypt(encrypted_data, secret_key);
    std::cout << "Decrypted data: " << decrypted_data << std::endl;
}
