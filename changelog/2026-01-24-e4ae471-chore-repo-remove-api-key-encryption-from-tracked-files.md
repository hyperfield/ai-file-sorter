# 2026-01-24: chore(repo): remove api-key-encryption from tracked files

## Covered commits
- `e4ae471` `2026-01-24` `chore(repo): remove api-key-encryption from tracked files`

## Motivation
This maintenance commit kept the repository state coherent even though it was not a headline feature. Chore commits often synchronize metadata, ignore rules, repository hygiene, or supporting assets that later work depends on.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `D` `api-key-encryption/compile.sh`
- `D` `api-key-encryption/obfuscate_encrypt.cpp`

## What changed from what, why, and how
The commit made maintenance-oriented updates in `api-key-encryption/compile.sh`, `api-key-encryption/obfuscate_encrypt.cpp`. It changed the repository support state, metadata, or supporting files in the way described by `chore(repo): remove api-key-encryption from tracked files`.

Before this commit, the repository reflected the state immediately preceding `e4ae471`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/api-key-encryption/compile.sh b/api-key-encryption/compile.sh
--- a/api-key-encryption/compile.sh
+++ /dev/null
@@ -1,3 +0,0 @@
-#!/usr/bin/env bash
-
-g++ obfuscate_encrypt.cpp -Wall -o obfuscate_encrypt -lssl -lcrypto
diff --git a/api-key-encryption/obfuscate_encrypt.cpp b/api-key-encryption/obfuscate_encrypt.cpp
deleted file mode 100644
index 6dd644b..0000000
--- a/api-key-encryption/obfuscate_encrypt.cpp
+++ /dev/null
@@ -1,327 +0,0 @@
-#include <utility>
-#include <stddef.h>
-#include <string>
-#include <cstring>
-
-#ifdef _WIN32
-#include <cstdlib>
-
-// Need setenv() before including dotenv.h on Windows
-int setenv(const char *name, const char *value, int overwrite) {
-    if (!overwrite && getenv(name)) return 0;
-    return _putenv_s(name, value);
-}
-#endif
-
-#include "../app/include/external/dotenv.h"
-#include <stdexcept>
-#include <iostream>
-#include <random>
-#include <openssl/bio.h>
-#include <openssl/evp.h>
-#include <openssl/buffer.h>
-#include <openssl/rand.h>
-
-
-std::string base64_encode(const std::string& input) {
-    BIO *bio, *b64;
-    BUF_MEM *buffer_ptr;
-
-    b64 = BIO_new(BIO_f_base64());
-    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL); // Ignore newlines
-    bio = BIO_new(BIO_s_mem());
-    bio = BIO_push(b64, bio);
-
-    BIO_write(bio, input.c_str(), input.size());
-    BIO_flush(bio);
-    BIO_get_mem_ptr(bio, &buffer_ptr);
-
-    std::string encoded(buffer_ptr->data, buffer_ptr->length);
-    BIO_free_all(bio);
-
-    return encoded;
-}
-
-
-std::string base64_decode(const std::string& encoded) {
-    BIO *bio, *b64;
-    int decode_len = encoded.size() * 3 / 4; // Approximation of the decoded size
-    char *buffer = new char[decode_len + 1]; // +1 for the null character
-    memset(buffer, 0, decode_len + 1);
-
-    b64 = BIO_new(BIO_f_base64());
-    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL); // Ignore newlines
-    bio = BIO_new_mem_buf(encoded.c_str(), -1); // Creation of buffer
-    bio = BIO_push(b64, bio);
-
-    int decoded_length = BIO_read(bio, buffer, decode_len);
-    if (decoded_length < 0) {
-        delete[] buffer;
-        BIO_free_all(bio);
-        throw std::runtime_error("Could not decode Base64.");
-    }
-
-    BIO_free_all(bio);
-
-    std::string decoded(buffer, decoded_length);
-    delete[] buffer;
-    return decoded;
-}
-
-
-std::pair<std::string, std::string> decompose_key(const std::string& api_key)
-{
-    // Ensure the key is long enough to split
-    if (api_key.length() < 2) {
-        throw std::invalid_argument("API key is too short to split.");
-    }
-
-    // Split the key into two halves
-    size_t mid = api_key.length() / 2;
-    std::string part1 = api_key.substr(0, mid);
-    std::string part2 = api_key.substr(mid);
-
-    return std::make_pair(part1, part2);
-}
-
-
```

The excerpt is taken from the commit diff for `chore(repo): remove api-key-encryption from tracked files`. The most relevant surfaces are `api-key-encryption/compile.sh`, `api-key-encryption/obfuscate_encrypt.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
