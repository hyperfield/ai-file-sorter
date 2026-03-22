#pragma once

namespace UpdaterLaunchOptions {

inline constexpr const char* kLiveTestFlag = "--updater-live-test";
inline constexpr const char* kLiveTestUrlFlag = "--updater-live-test-url=";
inline constexpr const char* kLiveTestSha256Flag = "--updater-live-test-sha256=";
inline constexpr const char* kLiveTestVersionFlag = "--updater-live-test-version=";
inline constexpr const char* kLiveTestMinVersionFlag = "--updater-live-test-min-version=";

inline constexpr const char* kLiveTestModeEnv = "AI_FILE_SORTER_UPDATER_TEST_MODE";
inline constexpr const char* kLiveTestUrlEnv = "AI_FILE_SORTER_UPDATER_TEST_URL";
inline constexpr const char* kLiveTestSha256Env = "AI_FILE_SORTER_UPDATER_TEST_SHA256";
inline constexpr const char* kLiveTestVersionEnv = "AI_FILE_SORTER_UPDATER_TEST_VERSION";
inline constexpr const char* kLiveTestMinVersionEnv = "AI_FILE_SORTER_UPDATER_TEST_MIN_VERSION";

} // namespace UpdaterLaunchOptions
