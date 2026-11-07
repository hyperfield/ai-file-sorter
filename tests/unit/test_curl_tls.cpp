#include "CurlTls.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/reporters/catch_reporter_event_listener.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>
#include <QFile>
#include <stdexcept>

#ifndef _WIN32
namespace {
class PlatformCurlInitialization : public Catch::EventListenerBase {
public:
    using Catch::EventListenerBase::EventListenerBase;

    void testRunStarting(const Catch::TestRunInfo&) override { CurlTls::initialize(); }
    void testRunEnded(const Catch::TestRunStats&) override { curl_global_cleanup(); }
};
} // namespace
CATCH_REGISTER_LISTENER(PlatformCurlInitialization)
#endif

TEST_CASE("Apple TLS trust requires a native verification backend", "[tls]")
{
    curl_version_info_data version{};
    version.age = CURLVERSION_NOW;
    version.features = CURL_VERSION_SSL;

    CHECK_FALSE(CurlTls::supports_apple_trust(nullptr));
    CHECK_FALSE(CurlTls::supports_apple_trust(&version));

    version.ssl_version = "OpenSSL/3.0.2";
    CHECK_FALSE(CurlTls::supports_apple_trust(&version));
    version.ssl_version = "SecureTransport";
    CHECK(CurlTls::supports_apple_trust(&version));
    version.ssl_version = "(SecureTransport) LibreSSL/3.3.6";
    CHECK_FALSE(CurlTls::supports_apple_trust(&version));
    version.ssl_version = "(OpenSSL/3.6.0) SecureTransport";
    CHECK(CurlTls::supports_apple_trust(&version));
    version.features = 0;
    CHECK_FALSE(CurlTls::supports_apple_trust(&version));

#if LIBCURL_VERSION_NUM >= 0x075700
    version.features = CURL_VERSION_SSL;
    version.ssl_version = "OpenSSL/3.6.0";
    const char* features[] = {"SSL", "AppleSecTrust", nullptr};
    version.feature_names = features;
    // AppleSecTrust does not prevent fallback to OpenSSL's default CA locations.
    CHECK_FALSE(CurlTls::supports_apple_trust(&version));
    version.ssl_version = "wolfSSL/5.8.0";
    CHECK_FALSE(CurlTls::supports_apple_trust(&version));
    version.ssl_version = "OpenSSL/3.6.0";
    // An older runtime does not own the newer fields, even with newer headers.
    version.age = CURLVERSION_TENTH;
    CHECK_FALSE(CurlTls::supports_apple_trust(&version));
#endif
}

#ifndef _WIN32
TEST_CASE("POSIX builds do not embed the Windows certificate bundle", "[tls]")
{
    CHECK_FALSE(QFile::exists(QStringLiteral(":/dev/hfstudio/AIFileSorter/certs/cacert.pem")));
}
#endif

TEST_CASE("TLS policy rejects an uninitialized handle", "[tls]")
{
    CHECK_THROWS_AS(CurlTls::configure(nullptr), std::invalid_argument);
}
