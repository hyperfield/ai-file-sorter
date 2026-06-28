#include <catch2/catch_test_macros.hpp>

#include "ProtectedProjectDetector.hpp"
#include "TestHelpers.hpp"
#include "Utils.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

std::string utf8_string(const char8_t* value)
{
    const auto* begin = reinterpret_cast<const char*>(value);
    return std::string(begin, begin + std::char_traits<char8_t>::length(value));
}

void write_file(const std::filesystem::path& path)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path);
    out << "data";
}

void create_directories(const std::filesystem::path& root,
                        const std::vector<std::filesystem::path>& paths)
{
    for (const auto& path : paths) {
        std::filesystem::create_directories(root / path);
    }
}

void write_files(const std::filesystem::path& root,
                 const std::vector<std::filesystem::path>& paths)
{
    for (const auto& path : paths) {
        write_file(root / path);
    }
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

TEST_CASE("ProtectedProjectDetector covers every built-in project rule")
{
    struct Fixture {
        std::string id;
        ProtectedProjectStrength strength;
        std::vector<std::filesystem::path> directories;
        std::vector<std::filesystem::path> files;
    };

    const std::vector<Fixture> fixtures = {
        {"unity", ProtectedProjectStrength::Strong, {"Assets"}, {"ProjectSettings/ProjectVersion.txt"}},
        {"unreal", ProtectedProjectStrength::Strong, {"Config", "Content"}, {"Game.uproject"}},
        {"godot", ProtectedProjectStrength::Strong, {}, {"project.godot"}},
        {"git", ProtectedProjectStrength::Strong, {".git"}, {}},
        {"node", ProtectedProjectStrength::Strong, {}, {"package.json", "pnpm-lock.yaml"}},
        {"python", ProtectedProjectStrength::Strong, {}, {"pyproject.toml"}},
        {"rust", ProtectedProjectStrength::Strong, {}, {"Cargo.toml"}},
        {"go", ProtectedProjectStrength::Strong, {}, {"go.mod"}},
        {"gradle", ProtectedProjectStrength::Strong, {}, {"settings.gradle", "build.gradle"}},
        {"dotnet", ProtectedProjectStrength::Strong, {}, {"App.sln"}},
        {"xcode", ProtectedProjectStrength::Strong, {"App.xcodeproj"}, {}},
        {"blender", ProtectedProjectStrength::Strong, {}, {"scene.blend", "textures/wood.png"}},
        {"blender-file", ProtectedProjectStrength::Weak, {}, {"scene.blend"}}
    };

    ProtectedProjectDetector detector;
    for (const auto& fixture : fixtures) {
        INFO("rule id: " << fixture.id);
        TempDir temp_dir;
        create_directories(temp_dir.path(), fixture.directories);
        write_files(temp_dir.path(), fixture.files);

        const auto match = detector.detect(temp_dir.path());

        REQUIRE(match.has_value());
        CHECK(match->id == fixture.id);
        CHECK(match->strength == fixture.strength);
        CHECK(ProtectedProjectDetector::should_skip(*match) ==
              (fixture.strength == ProtectedProjectStrength::Strong));
    }
}

TEST_CASE("ProtectedProjectDetector ignores Unicode loose files while checking suffix rules")
{
    TempDir temp_dir;
    write_file(temp_dir.path() / Utils::utf8_to_path(utf8_string(u8"旅行.txt")));

    ProtectedProjectDetector detector;

    CHECK_FALSE(detector.detect(temp_dir.path()).has_value());
}
