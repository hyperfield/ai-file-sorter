#include <catch2/catch_test_macros.hpp>

#include "ProtectedProjectDetector.hpp"
#include "TestHelpers.hpp"

#include <filesystem>
#include <fstream>

namespace {

void write_file(const std::filesystem::path& path)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path);
    out << "data";
}

} // namespace

TEST_CASE("ProtectedProjectDetector detects strong Unity roots")
{
    TempDir temp_dir;
    const auto root = temp_dir.path() / "UnityGame";
    std::filesystem::create_directories(root / "Assets");
    write_file(root / "ProjectSettings" / "ProjectVersion.txt");

    ProtectedProjectDetector detector;
    const auto match = detector.detect(root);

    REQUIRE(match.has_value());
    CHECK(match->id == "unity");
    CHECK(match->name == "Unity project");
    CHECK(match->strength == ProtectedProjectStrength::Strong);
    CHECK(ProtectedProjectDetector::should_skip(*match));
}

TEST_CASE("ProtectedProjectDetector detects conservative Blender project folders")
{
    TempDir temp_dir;
    const auto root = temp_dir.path() / "BlenderScene";
    write_file(root / "scene.blend");
    write_file(root / "textures" / "wood.png");

    ProtectedProjectDetector detector;
    const auto match = detector.detect(root);

    REQUIRE(match.has_value());
    CHECK(match->id == "blender");
    CHECK(match->strength == ProtectedProjectStrength::Strong);
    CHECK(ProtectedProjectDetector::should_skip(*match));
}

TEST_CASE("ProtectedProjectDetector treats lone Blender files as weak signals")
{
    TempDir temp_dir;
    const auto root = temp_dir.path() / "LooseBlend";
    write_file(root / "scene.blend");

    ProtectedProjectDetector detector;
    const auto match = detector.detect(root);

    REQUIRE(match.has_value());
    CHECK(match->id == "blender-file");
    CHECK(match->strength == ProtectedProjectStrength::Weak);
    CHECK_FALSE(ProtectedProjectDetector::should_skip(*match));
}

TEST_CASE("ProtectedProjectDetector detects common source project roots")
{
    TempDir temp_dir;
    const auto root = temp_dir.path() / "Repo";
    write_file(root / "package.json");
    write_file(root / "pnpm-lock.yaml");

    ProtectedProjectDetector detector;
    const auto match = detector.detect(root);

    REQUIRE(match.has_value());
    CHECK(match->id == "node");
    CHECK(match->strength == ProtectedProjectStrength::Strong);
}
