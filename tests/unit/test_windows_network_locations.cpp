#include <catch2/catch_test_macros.hpp>

#include "WindowsNetworkLocations.hpp"

#ifdef _WIN32
TEST_CASE("WindowsNetworkLocations extracts UNC share roots")
{
    CHECK(WindowsNetworkLocations::unc_share_root("\\\\server\\share\\folder\\file.txt") ==
          "\\\\server\\share");
    CHECK(WindowsNetworkLocations::unc_share_root("//server/share/folder") ==
          "\\\\server\\share");
    CHECK(WindowsNetworkLocations::unc_share_root("\\\\server") == "");
    CHECK(WindowsNetworkLocations::unc_share_root("C:\\Users") == "");
}

TEST_CASE("WindowsNetworkLocations identifies supported network paths")
{
    CHECK(WindowsNetworkLocations::is_unc_path("\\\\server\\share"));
    CHECK(WindowsNetworkLocations::is_network_location_path("\\\\server\\share"));
    CHECK_FALSE(WindowsNetworkLocations::is_network_location_path("C:\\"));
}
#endif
