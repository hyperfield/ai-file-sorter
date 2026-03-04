/**
 * @file SupportCodeManager.hpp
 * @brief Offline donation-code validation and prompt suppression helpers.
 */
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

/**
 * @brief Handles offline donation-code validation and binary prompt suppression state.
 */
class SupportCodeManager {
public:
    /**
     * @brief Constructs a support-code manager rooted at the app config directory.
     * @param config_dir Base configuration directory used for the binary suppression blob.
     */
    explicit SupportCodeManager(std::filesystem::path config_dir);

    /**
     * @brief Returns whether a donation code passes offline validation.
     * @param code User-provided donation code.
     * @return True when the code is valid.
     */
    static bool is_valid_code(const std::string& code);

    /**
     * @brief Validates and stores a donation code so the support prompt stays hidden.
     * @param code User-provided donation code.
     * @return True when the code is valid and the suppression blob was written.
     */
    bool redeem_code(const std::string& code) const;

    /**
     * @brief Returns whether the binary suppression blob is present and valid.
     * @return True when the support prompt should remain hidden.
     */
    bool is_prompt_permanently_disabled() const;

#ifdef AI_FILE_SORTER_TEST_BUILD
    /**
     * @brief Writes a valid suppression blob without requiring a real donation code.
     * @return True when the test suppression blob was written successfully.
     */
    bool force_disable_prompt_for_testing() const;
#endif

private:
    /**
     * @brief Verifies a donation code and returns the signed payload on success.
     * @param code User-provided donation code.
     * @return Signed payload bytes when valid, otherwise `std::nullopt`.
     */
    static std::optional<std::string> decode_payload(const std::string& code);

    /**
     * @brief Resolves the binary suppression blob path.
     * @return Filesystem path used for the stored suppression state.
     */
    std::filesystem::path storage_path() const;

    /**
     * @brief Derives the current machine binding key.
     * @return Opaque machine-bound key material.
     */
    std::string machine_binding_key() const;

    /**
     * @brief Writes an opaque suppression blob for the provided signed payload.
     * @param payload Signed payload from a valid donation code.
     * @return True when the blob was written successfully.
     */
    bool write_state(const std::string& payload) const;

    std::filesystem::path config_dir_;
};
