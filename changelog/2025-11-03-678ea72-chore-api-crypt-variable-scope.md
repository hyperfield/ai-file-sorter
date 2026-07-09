# 2025-11-03: chore(api-crypt): variable scope

## Covered commits
- `678ea72` `2025-11-03` `chore(api-crypt): variable scope`

## Motivation
This maintenance commit kept the repository state coherent even though it was not a headline feature. Chore commits often synchronize metadata, ignore rules, repository hygiene, or supporting assets that later work depends on.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `api-key-encryption/obfuscate_encrypt.cpp`
- `M` `app/lib/CryptoManager.cpp`

## What changed from what, why, and how
The commit made maintenance-oriented updates in `api-key-encryption/obfuscate_encrypt.cpp`, `app/lib/CryptoManager.cpp`. It changed the repository support state, metadata, or supporting files in the way described by `chore(api-crypt): variable scope`.

Before this commit, the repository reflected the state immediately preceding `678ea72`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/api-key-encryption/obfuscate_encrypt.cpp b/api-key-encryption/obfuscate_encrypt.cpp
--- a/api-key-encryption/obfuscate_encrypt.cpp
+++ b/api-key-encryption/obfuscate_encrypt.cpp
@@ -180,7 +180,7 @@ std::vector<unsigned char> aes256_encrypt(const std::string& plaintext, const st
 
     // Output buffer
     std::vector<unsigned char> ciphertext(plaintext.size() + EVP_MAX_BLOCK_LENGTH);
-    int len = 0, ciphertext_len = 0;
+    int ciphertext_len = 0;
 
     // Create and initialize the context
     EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
@@ -199,6 +199,7 @@ std::vector<unsigned char> aes256_encrypt(const std::string& plaintext, const st
         }
 
         // Encrypt the plaintext
+        int len = 0;
         if (EVP_EncryptUpdate(ctx,
                               ciphertext.data(),
                               &len,
```

The excerpt is taken from the commit diff for `chore(api-crypt): variable scope`. The most relevant surfaces are `api-key-encryption/obfuscate_encrypt.cpp`, `app/lib/CryptoManager.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
