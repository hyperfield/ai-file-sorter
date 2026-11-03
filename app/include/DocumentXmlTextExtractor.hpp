#pragma once

#include <cstddef>
#include <filesystem>
#include <functional>
#include <initializer_list>
#include <string>

namespace DocumentXmlTextExtractor {
/**
 * @brief Stream Office XML until the useful UTF-8 text limit is reached.
 * @param archive Office document archive to inspect without modifying it.
 * @param members Candidate XML members, tried until one yields useful text.
 * @param max_text_bytes Maximum extracted UTF-8 bytes, excluding XML markup and styles.
 * @param should_stop Optional cooperative cancellation check.
 * @return Useful text, or an empty string when no readable member contains text.
 * @throws ContentAnalysisUnavailable When complete XML is malformed or too deeply nested.
 * @throws std::runtime_error On cancellation, scan limits, or stream read errors.
 * @note Skips style definitions, bounds raw XML reads, and never parses a truncated XML prefix as a document.
 */
std::string extract(const std::filesystem::path& archive, std::initializer_list<const char*> members,
                    std::size_t max_text_bytes, const std::function<bool()>& should_stop = {});
}
