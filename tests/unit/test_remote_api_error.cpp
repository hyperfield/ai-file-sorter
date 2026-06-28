#include <catch2/catch_test_macros.hpp>

#include "LLMErrors.hpp"
#include "RemoteApiError.hpp"

#include <stdexcept>
#include <string>

TEST_CASE("RemoteApiError parses numeric Retry-After headers")
{
    REQUIRE(RemoteApiError::parse_retry_after_seconds(" 20\r\n").value() == 20);
    REQUIRE(RemoteApiError::parse_retry_after_seconds("1.2").value() == 2);
    REQUIRE_FALSE(RemoteApiError::parse_retry_after_seconds("Wed, 21 Oct 2015 07:28:00 GMT").has_value());
}

TEST_CASE("RemoteApiError turns non-JSON 429 responses into BackoffError")
{
    try {
        RemoteApiError::throw_for_http_error("OpenRouter",
                                             429,
                                             "Too many requests",
                                             "17",
                                             nullptr);
        FAIL("Expected BackoffError");
    } catch (const BackoffError& ex) {
        REQUIRE(ex.retry_after_seconds() == 17);
        REQUIRE(std::string(ex.what()).find("rate limit") != std::string::npos);
        REQUIRE(std::string(ex.what()).find("Too many requests") != std::string::npos);
    }
}

TEST_CASE("RemoteApiError extracts Gemini quota retry delays from JSON")
{
    const std::string payload =
        R"({"error":{"code":429,"status":"RESOURCE_EXHAUSTED","message":"Quota exceeded. Please retry in 2.4s"}})";

    try {
        RemoteApiError::throw_for_http_error("Gemini", 429, payload, "", nullptr);
        FAIL("Expected BackoffError");
    } catch (const BackoffError& ex) {
        REQUIRE(ex.retry_after_seconds() == 3);
        REQUIRE(std::string(ex.what()).find("RESOURCE_EXHAUSTED") != std::string::npos);
    }
}

TEST_CASE("RemoteApiError reports non-JSON server errors without JSON parse failures")
{
    try {
        RemoteApiError::throw_for_http_error("Remote LLM",
                                             503,
                                             "<html>temporary outage</html>",
                                             "",
                                             nullptr);
        FAIL("Expected runtime_error");
    } catch (const BackoffError&) {
        FAIL("Expected non-retryable runtime_error");
    } catch (const std::runtime_error& ex) {
        const std::string message = ex.what();
        REQUIRE(message.find("Server Error") != std::string::npos);
        REQUIRE(message.find("HTTP 503") != std::string::npos);
        REQUIRE(message.find("temporary outage") != std::string::npos);
        REQUIRE(message.find("parse JSON") == std::string::npos);
    }
}
