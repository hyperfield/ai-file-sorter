#include <catch2/catch_test_macros.hpp>

#include "hash/hash.h"

TEST_CASE("Bundled llama SHA-256 remains correct alongside MediaInfo")
{
    CHECK(hash_sha256_hex("", 0) ==
          "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    CHECK(hash_sha256_hex("abc", 3) ==
          "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}
