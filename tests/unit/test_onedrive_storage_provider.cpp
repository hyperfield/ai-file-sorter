#include "OneDrivePlatform.hpp"
#include "NativeSyncStorageProvider.hpp"
#include "TestHelpers.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QTemporaryDir>

#ifdef __APPLE__
#include "OneDriveMacMutations.hpp"
#include "OneDriveMacRootDetection.hpp"

#include <sys/stat.h>
#include <sys/xattr.h>

#include <array>
#include <map>
#endif

#include <chrono>
#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

#ifdef _WIN32
#include <windows.h>
#include <winternl.h>
#include <cfapi.h>
#endif

namespace {

using one_drive::SyncProvider;

constexpr auto kFiles = FileScanOptions::Files;
constexpr auto kDirectories = FileScanOptions::Directories;
constexpr auto kHidden = FileScanOptions::HiddenFiles;
constexpr auto kRecursive = FileScanOptions::Recursive;

class FakePlatformProbe final : public one_drive::PlatformProbe {
public:
  std::optional<one_drive::SyncRootDetection> detection;
  one_drive::PlatformFileInfo file_info;

  one_drive::SyncRootDetection
  detect_sync_root(const std::filesystem::path &) const override {
    return detection.value_or(one_drive::SyncRootDetection{});
  }

  one_drive::PathInspection inspect_path(const std::filesystem::path &path) const override {
    auto inspection = PlatformProbe::inspect_path(path);
    inspection.state = file_info;
    return inspection;
  }
};

[[noreturn]] void fail(std::string_view message) {
  throw std::runtime_error(std::string(message));
}

void require(bool condition, std::string_view message) {
  if (!condition) {
    fail(message);
  }
}

std::string path_text(const std::filesystem::path &path) {
  return one_drive::path_to_qstring(path).toStdString();
}

void write_file(const std::filesystem::path &path, std::string_view contents) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary);
  out.write(contents.data(), static_cast<std::streamsize>(contents.size()));
}

TEST_CASE("OneDrive authoritative onedrive detection", "[onedrive]") {
  FakePlatformProbe probe;
  probe.detection = one_drive::SyncRootDetection{
      .matched = true,
      .authoritative = true,
      .vetoed = false,
      .confidence = 95,
      .source = QStringLiteral("windows_cloud_files"),
      .provider_name = QStringLiteral("Microsoft OneDrive"),
      .message = QStringLiteral("Detected OneDrive via Windows Cloud Files.")};

  const NativeSyncStorageProvider detector(SyncProvider::OneDrive, true, probe);
  const auto detection = detector.detect("C:/Neutral/Folder");
  require(detection.matched && detection.needs_additional_support &&
              detection.detection_source == "windows_cloud_files",
          "authoritative OneDrive root or detection source was lost");
}

TEST_CASE("OneDrive authoritative non onedrive vetoes path heuristic", "[onedrive]") {
  FakePlatformProbe probe;
  probe.detection = one_drive::SyncRootDetection{
      .matched = false,
      .authoritative = true,
      .vetoed = true,
      .confidence = 0,
      .source = QStringLiteral("windows_cloud_files"),
      .provider_name = QStringLiteral("Dropbox"),
      .message = QStringLiteral(
          "Windows Cloud Files identified sync provider 'Dropbox'.")};

  require(!NativeSyncStorageProvider(SyncProvider::OneDrive, false, probe).detect("C:/Users/example/OneDrive - Work/Documents").matched,
          "non-OneDrive authoritative provider did not veto heuristic match");
}

#ifdef __APPLE__
TEST_CASE("OneDrive macos file provider root detection", "[onedrive]") {
  namespace fs = std::filesystem;
  QTemporaryDir fixture;
  require(fixture.isValid(), "could not create macOS detection fixture");
  const auto base = fs::canonical(one_drive::path_from_qstring(fixture.path()));
  const auto business = base / "External Volume" / "Work Documents";
  const auto personal = base / "Personal";
  const auto lookalike = base / "OneDrive-Personal";
  const auto other_provider = business / "OneDrive-Lookalike";
  const auto unavailable = business / "Unavailable Metadata";
  const auto malformed = business / "Malformed Metadata";
  const auto child = business / "Nested" / one_drive::path_from_qstring(QStringLiteral("資料"));
  const auto alias = base / "Sync Folder Link";
  const auto escape = business / "OneDrive Escape";

  for (const auto &directory : {personal, lookalike, other_provider, unavailable,
                                malformed, child}) {
    fs::create_directories(directory);
  }
  fs::create_directory_symlink(business, alias);
  fs::create_directory_symlink(lookalike, escape);

  const std::map<fs::path, QByteArray> domains{
      {business, "com.microsoft.OneDrive.FileProvider/business-domain"},
      {personal, "com.microsoft.OneDrive-mac.FileProvider/OneDrive"},
      {other_provider, "com.microsoft.OneDrive.FileProviderOther/OneDrive"},
      {malformed, "com.microsoft.OneDrive.FileProvider/"},
  };
  const auto detect = [&](const fs::path &path) {
    return one_drive::detect_macos_onedrive_root(
        path, [&](const fs::path &directory, std::error_code &error)
                  -> std::optional<QByteArray> {
          if (directory == unavailable) {
            error = std::make_error_code(std::errc::permission_denied);
            return std::nullopt;
          }
          const auto domain = domains.find(directory);
          if (domain != domains.end()) {
            return domain->second;
          }
          return std::nullopt;
        });
  };

  for (const auto &path : {business, personal, child, alias,
                          alias / child.lexically_relative(business)}) {
    const auto detection = detect(path);
    require(detection.matched && detection.authoritative && !detection.vetoed &&
                detection.source == QStringLiteral("macos_file_provider"),
            "OneDrive File Provider domain was not matched");
    require(!detection.message.contains(QStringLiteral("business-domain")),
            "detection exposed the account domain identifier");
  }

  const auto other = detect(other_provider);
  require(!other.matched && other.authoritative && other.vetoed,
          "the nearest File Provider must veto an outer OneDrive match");
  for (const auto &path : {lookalike, escape, malformed, unavailable,
                          business / "Missing Folder"}) {
    require(!detect(path).matched, "unverified folder matched OneDrive");
  }
}

TEST_CASE("OneDrive macos local move blockers", "[onedrive]") {
  QTemporaryDir fixture;
  require(fixture.isValid(), "could not create macOS inspection fixture");
  const auto root = one_drive::path_from_qstring(fixture.path());
  const auto source = root / "report.txt";
  const auto locked = root / "locked.txt";
  write_file(source, "fixture");
  write_file(locked, "fixture");

  constexpr auto collision_attribute = "com.apple.fileprovider.before-bounce#PX";
  constexpr std::string_view original_name = "Report.txt";
  require(setxattr(source.c_str(), collision_attribute, original_name.data(),
                   original_name.size(), 0, 0) == 0,
          "could not simulate File Provider filename collision metadata");
  require(chflags(locked.c_str(), UF_IMMUTABLE) == 0,
          "could not make the macOS test file immutable");
  const auto lock_status = one_drive::inspect_onedrive_path(locked).state;
  require(chflags(locked.c_str(), 0) == 0, "could not unlock the macOS test file");
  require(lock_status.move_blocked && lock_status.sync_locked && !lock_status.should_retry,
          "immutable file must require the user to remove the lock");

  const NativeSyncStorageProvider provider(SyncProvider::OneDrive);
  const auto collision = provider.preflight_move(path_text(source), path_text(root / "moved.txt"));
  require(!collision.allowed && collision.source_status.conflict_copy,
          "native filename conflict must block preflight independently of the filename");

  const auto invalid_parent = provider.preflight_move(path_text(locked), path_text(source / "child.txt"));
  require(!invalid_parent.allowed && !invalid_parent.message.empty(),
          "an invalid destination parent must not be mistaken for a missing destination");
}
TEST_CASE("OneDrive macos coordinated move and undo", "[onedrive]") {
  namespace fs = std::filesystem;
  QTemporaryDir fixture;
  require(fixture.isValid(), "could not create macOS move fixture");
  const auto root = one_drive::path_from_qstring(fixture.path());
  const auto actual = root / "Actual";
  const auto linked = root / "Linked";
  const auto unrelated = root / "Unrelated";
  for (const auto &directory : {actual, unrelated}) {
    fs::create_directory(directory);
  }
  fs::create_directory_symlink(actual, linked);
  const auto source = linked / "report.txt";
  const auto target = linked / "Created" / "Nested" / "report.txt";
  write_file(source, "one");
  write_file(target, "different length");

  // Exercise collision handling inside the native operation, independently of
  // the connector's earlier preflight check.
  const auto collision = one_drive::move_macos_path(source, target, {});
  require(collision.skipped && !collision.error_code.isEmpty() &&
              fs::file_size(source) == 3 && fs::file_size(target) == 16,
          "coordinated rename replaced an existing destination or lost the source");

  fs::remove_all(actual / "Created");
  const NativeSyncStorageProvider provider(SyncProvider::OneDrive);
  const auto moved = provider.move_entry(path_text(source), path_text(target));
  require(moved.success && moved.metadata.size_bytes == 3 && !fs::exists(source) && fs::file_size(target) == 3,
          "coordinated move through a linked root did not create the missing parents");

  auto recorded_parent = target.parent_path();
  auto recorded_grandparent = target.parent_path().parent_path();
  // The undo plan may use different path spelling for the same directories.
  const auto alternate_parent = linked / "created" / "nested";
  std::error_code equivalent_error;
  if (fs::equivalent(recorded_parent, alternate_parent, equivalent_error)) {
    recorded_parent = alternate_parent;
    recorded_grandparent = alternate_parent.parent_path();
  }
  const std::vector<std::string> recorded{
      path_text(recorded_parent), path_text(recorded_grandparent), path_text(unrelated)};
  write_file(source, "occupied");
  require(!provider.undo_move(path_text(source), path_text(target), recorded).success &&
              fs::file_size(source) == 8 && fs::file_size(target) == 3,
          "undo ran despite a collision at the original path");
  fs::remove(source);

  require(provider.undo_move(path_text(source), path_text(target), recorded).success &&
              fs::file_size(source) == 3 && !fs::exists(actual / "Created") &&
              fs::exists(unrelated) && fs::is_symlink(linked),
          "undo did not restore the file and remove only the recorded empty parents");
}

TEST_CASE("OneDrive macos content revision verifies undo", "[onedrive]") {
  namespace fs = std::filesystem;
  using namespace std::chrono_literals;
  QTemporaryDir fixture;
  require(fixture.isValid(), "could not create macOS revision fixture");
  const auto root = one_drive::path_from_qstring(fixture.path());
  const auto source = root / "source.txt";
  const auto destination = root / "destination.txt";
  write_file(source, "original");

  const NativeSyncStorageProvider provider(SyncProvider::OneDrive);
  const auto before = provider.inspect_path(path_text(source));
  const auto moved = provider.move_entry(path_text(source), path_text(destination));
  require(moved.success, "macOS revision fixture move failed");
  const auto &metadata = moved.metadata;
  require(metadata.stable_identity.starts_with("onedrive:mac-fileid:") &&
              metadata.stable_identity == before.stable_identity,
          "native file identity changed across a same-volume move");

  const auto native = one_drive::inspect_onedrive_path(destination).state;
  require(native.revision_suffix.startsWith(QStringLiteral("mac-write-v1:")) &&
              native.can_capture_content_revision,
          "the moved file does not have the expected native revision");
  const auto &fingerprint = metadata.revision_token;
  require(fingerprint.starts_with("onedrive:sha256:") && fingerprint.size() == 80,
          "move did not capture the file's SHA-256 revision");
  require(one_drive::local_content_revision(destination, QStringLiteral("different file"),
                                            native.revision_suffix).isEmpty(),
          "capture did not check the moved file's identity");
  require(one_drive::local_content_revision(destination, native.stable_identity,
                                            native.revision_suffix + "stale").isEmpty(),
          "capture did not check the moved file's revision");

  const auto replacement = root / "replacement.txt";
  write_file(replacement, "original");
  const auto replacement_time = fs::last_write_time(destination) + 5s;
  fs::last_write_time(replacement, replacement_time);
  fs::rename(replacement, destination);
  require(one_drive::local_content_revision(destination, native.stable_identity,
                                            native.revision_suffix).isEmpty(),
          "capture accepted a local copy that replaced the moved file");

  const auto same_content = provider.inspect_for_undo(path_text(destination), fingerprint);
  require(same_content.content_matches == true &&
              same_content.status.stable_identity != metadata.stable_identity,
          "byte-identical replacement did not verify after its native metadata changed");

  write_file(destination, "modified");
  fs::last_write_time(destination, replacement_time);
  const auto edited = provider.inspect_for_undo(path_text(destination), fingerprint);
  require(edited.content_matches == false && edited.status.revision_token != same_content.status.revision_token,
          "equal-length edit with restored mtime escaped revision validation");

  const auto symlink = root / "symlink.txt";
  write_file(source, "original");
  fs::create_symlink(source, symlink);
  require(provider.inspect_for_undo(path_text(symlink), fingerprint).content_matches == false,
          "content verification followed a symlink");
}
#endif

TEST_CASE("OneDrive platform hydration status is reported", "[onedrive]") {
  QTemporaryDir fixture;
  require(fixture.isValid(), "could not create hydration fixture");
  const auto file_path = one_drive::path_from_qstring(fixture.path()) / "source.txt";
  write_file(file_path, "fixture");

  FakePlatformProbe probe;
  probe.file_info.hydration_required = true;
  probe.file_info.should_retry = true;
  probe.file_info.move_blocked = true;
  probe.file_info.stable_identity = QStringLiteral("onedrive:cf:test-id");
  probe.file_info.revision_suffix = QStringLiteral("test-revision");
  probe.file_info.message =
      QStringLiteral("OneDrive has not fully hydrated this item yet.");

  const NativeSyncStorageProvider provider(SyncProvider::OneDrive, false, probe);
  const auto status = provider.inspect_path(path_text(file_path));
  require(status.exists && status.hydration_required && status.should_retry,
          "platform hydration and retry flags were not preserved");
  require(status.stable_identity == "onedrive:cf:test-id" && status.revision_token == "onedrive:test-revision",
          "platform identity and revision were not preserved");
  const auto destination = file_path.parent_path() / "unhydrated-move-target" / file_path.filename();
  const auto result = provider.move_entry(path_text(file_path), path_text(destination));
  require(!result.success && result.skipped && std::filesystem::exists(file_path) &&
              !std::filesystem::exists(destination),
          "a hydration blocker must prevent a direct move");
}

std::vector<FileEntry> scan(const std::filesystem::path &root, FileScanOptions options) {
  return NativeSyncStorageProvider(SyncProvider::OneDrive).list_directory(path_text(root), options);
}

TEST_CASE("OneDrive scan filters hidden entries and reports errors", "[onedrive]") {
  QTemporaryDir temp;
  require(temp.isValid(), "could not create scan fixture");
  const auto root = one_drive::path_from_qstring(temp.path());
  write_file(root / "visible" / "ordinary.txt", "fixture");
  write_file(root / ".hidden" / "secret.txt", "fixture");
  std::filesystem::create_directory(root / "empty");
  std::size_t all_files = 2;
#ifdef _WIN32
  const auto hidden_file = root / "native-hidden.txt";
  const auto hidden_dir = root / "native-hidden-dir";
  write_file(hidden_file, "fixture");
  write_file(hidden_dir / "secret.txt", "fixture");
  require(SetFileAttributesW(hidden_file.c_str(), FILE_ATTRIBUTE_HIDDEN),
          "could not hide fixture file");
  require(SetFileAttributesW(hidden_dir.c_str(), FILE_ATTRIBUTE_HIDDEN),
          "could not hide fixture directory");
  all_files += 2;
#endif

  const auto entries = scan(root, kFiles | kRecursive);
  require(entries.size() == 1 && entries[0].file_name == "ordinary.txt", "hidden entry escaped scan filtering");
  require(scan(root, kFiles | kHidden | kRecursive).size() == all_files,
          "hidden opt-in did not include files from hidden directories");

  for (auto options : {kFiles, kFiles | kRecursive}) {
    REQUIRE(scan(root / "empty", options).empty());
    REQUIRE_THROWS_AS(scan(root / "missing", options), std::filesystem::filesystem_error);
    REQUIRE_THROWS_AS(scan(root / "visible" / "ordinary.txt", options), std::filesystem::filesystem_error);
  }
}

TEST_CASE("OneDrive preserves move safeguards in the built-in provider", "[onedrive]") {
  namespace fs = std::filesystem;
  QTemporaryDir fixture;
  REQUIRE(fixture.isValid());
  const auto root = one_drive::path_from_qstring(fixture.path());
  const auto source = root / "source.txt";
  const auto locked = root / "~$locked.docx";
  const auto conflict = root / "budget conflicted copy.txt";
  const auto staging = root / ".tmp.drivedownload" / "pending.tmp";
  const auto destination = root / "Moved" / "source.txt";
  for (const auto &path : {source, locked, conflict, staging}) {
    write_file(path, "fixture");
  }
  const EnvVarGuard environment("OneDrive", path_text(root));
  const NativeSyncStorageProvider provider(SyncProvider::OneDrive);
#ifdef __APPLE__
  CHECK_FALSE(provider.detect(path_text(root)).matched);
#else
  CHECK(provider.detect(path_text(root)).matched);
#endif
  const auto lock = provider.preflight_move(path_text(locked), path_text(destination));
  CHECK_FALSE(lock.allowed);
  CHECK(lock.sync_locked);
  CHECK(lock.should_retry);
  const auto conflict_status = provider.inspect_path(path_text(conflict));
  CHECK(conflict_status.conflict_copy);
  CHECK(conflict_status.sync_locked);
  CHECK(provider.inspect_path(path_text(staging)).sync_locked);

  const auto prior = provider.inspect_path(path_text(source));
  const auto moved = provider.move_entry(path_text(source), path_text(destination));
  REQUIRE(moved.success);
  CHECK(fs::exists(destination));
#if defined(__APPLE__) || defined(_WIN32)
  CHECK(moved.metadata.stable_identity == prior.stable_identity);
#endif
  REQUIRE(provider.undo_move(path_text(source), path_text(destination), {path_text(destination.parent_path())}).success);
  CHECK(fs::exists(source));
  CHECK_FALSE(fs::exists(destination.parent_path()));
}

TEST_CASE("OneDrive excludes protected projects from recursive scans", "[onedrive]") {
  QTemporaryDir fixture;
  REQUIRE(fixture.isValid());
  const auto root = one_drive::path_from_qstring(fixture.path());
  const auto project = root / "Repository";
  write_file(project / ".git" / "HEAD", "ref: refs/heads/main");
  write_file(project / "project.txt", "fixture");
  write_file(root / "ordinary.txt", "fixture");

  const auto entries = scan(root, kFiles | kDirectories | kRecursive);
  REQUIRE(entries.size() == 1);
  CHECK(entries.front().file_name == "ordinary.txt");
  CHECK(scan(project, kFiles | kRecursive).empty());
}

#ifdef __APPLE__
void set_finder_flags(const std::filesystem::path &path, unsigned char flags) {
  std::array<unsigned char, 32> finder_info{};
  finder_info[8] = flags;
  require(setxattr(path.c_str(), "com.apple.FinderInfo", finder_info.data(),
                   finder_info.size(), 0, 0) == 0,
          "could not set fixture Finder flags");
}

TEST_CASE("OneDrive macos scan preserves names without following links", "[onedrive]") {
  namespace fs = std::filesystem;
  QTemporaryDir temp;
  require(temp.isValid(), "could not create macOS scan fixture");
  const auto base = one_drive::path_from_qstring(temp.path());
  const auto root = base / "Actual";
  const auto selected = base / "Selected Link";
  const auto outside = base / "Outside";
  const auto name = one_drive::path_from_qstring(QStringLiteral("Café 日本語 😀.txt"));
  const auto hidden_dir = root / "Finder Hidden";
  const auto hidden_file = root / "Invisible.txt";
  const auto finder_alias = root / "Alias";
  write_file(root / name, "fixture");
  write_file(hidden_dir / "nested.txt", "fixture");
  write_file(hidden_file, "fixture");
  write_file(outside / "outside.txt", "fixture");
  write_file(finder_alias, "fixture");
  require(chflags(hidden_dir.c_str(), UF_HIDDEN) == 0,
          "could not mark a fixture directory hidden");
  set_finder_flags(hidden_file, 0x40);
  set_finder_flags(finder_alias, 0x80);
  fs::create_directory_symlink(root, selected);
  fs::create_directory_symlink(outside, root / "Outside Link");
  fs::create_symlink(outside / "outside.txt", root / "File Link");

  const auto visible = scan(selected, kFiles | kDirectories | kRecursive);
  REQUIRE(visible.size() == 1);
  const auto &entry = visible.front();
  require(entry.file_name == path_text(name) && entry.full_path == path_text(selected / name) &&
              fs::exists(one_drive::path_from_qstring(QString::fromStdString(entry.full_path))),
          "scan lost a Unicode name or crossed a hidden folder or link");

  REQUIRE(scan(selected, kFiles | kDirectories | kHidden | kRecursive).size() == 4);
  const NativeSyncStorageProvider provider(SyncProvider::OneDrive);
  const auto invalid = path_text(selected) + '\0' + "suffix";
  REQUIRE_THROWS_AS(provider.list_directory(invalid, kFiles), std::invalid_argument);
  REQUIRE_THROWS_AS(scan(finder_alias, kFiles), std::filesystem::filesystem_error);

  const auto result = provider.preflight_move(path_text(finder_alias), path_text(root / "Moved Alias"));
  require(!result.allowed && result.message.find("Finder aliases") != std::string::npos,
          "preflight did not protect a Finder alias");
}
#endif

TEST_CASE("Dropbox native provider needs authoritative sync-root detection", "[dropbox]") {
  QTemporaryDir fixture;
  REQUIRE(fixture.isValid());
  const auto root = one_drive::path_from_qstring(fixture.path()) / "Dropbox";
  std::filesystem::create_directory(root);
  CHECK_FALSE(NativeSyncStorageProvider(SyncProvider::Dropbox).detect(path_text(root)).matched);

  FakePlatformProbe probe;
  const NativeSyncStorageProvider provider(SyncProvider::Dropbox, false, probe);
  probe.detection = one_drive::SyncRootDetection{
      .matched = true, .confidence = 80, .source = QStringLiteral("path_heuristic")};
  CHECK_FALSE(provider.detect(path_text(root)).matched);
  probe.detection->authoritative = true;
  probe.detection->source = QStringLiteral("fixture_sync_root");
  CHECK(provider.detect(path_text(root)).provider_id == NativeSyncStorageProvider::dropbox_provider_id());
  probe.detection->vetoed = true;
  CHECK_FALSE(provider.detect(path_text(root)).matched);
}

TEST_CASE("Dropbox protects online-only files and client metadata", "[dropbox]") {
  QTemporaryDir fixture;
  REQUIRE(fixture.isValid());
  const auto root = one_drive::path_from_qstring(fixture.path());
  const auto source = root / "source.txt";
  const auto destination = root / "sorted" / "source.txt";
  const auto conflict = root / "notes (Alex's conflicted copy).txt";
  const auto cache = root / ".dropbox.cache";
  write_file(source, "fixture");
  write_file(conflict, "conflicting edit");
  write_file(cache / "private.txt", "sync state");

  FakePlatformProbe probe;
  probe.file_info.hydration_required = true;
  const NativeSyncStorageProvider offline(SyncProvider::Dropbox, false, probe);
  const auto preflight = offline.preflight_move(path_text(source), path_text(destination));
  CHECK(preflight.hydration_required);
  CHECK_FALSE(preflight.allowed);
  CHECK_FALSE(offline.move_entry(path_text(source), path_text(destination)).success);
  CHECK_FALSE(std::filesystem::exists(destination.parent_path()));

  const NativeSyncStorageProvider provider(SyncProvider::Dropbox);
  CHECK(provider.inspect_path(path_text(conflict)).conflict_copy);
  CHECK_FALSE(provider.preflight_move(path_text(conflict), path_text(destination)).allowed);
  CHECK_FALSE(provider.preflight_move(path_text(cache / "private.txt"), path_text(destination)).allowed);
  CHECK_FALSE(provider.ensure_directory(path_text(cache / "sorted")));
  CHECK_FALSE(std::filesystem::exists(cache / "sorted"));
  CHECK(provider.list_directory(path_text(cache), kFiles | kHidden | kRecursive).empty());
  const auto entries = provider.list_directory(path_text(root), kFiles | kDirectories | kRecursive | kHidden);
  CHECK(entries.size() == 2);

  const auto moved = provider.move_entry(path_text(source), path_text(destination));
  REQUIRE(moved.success);
  CHECK(moved.metadata.stable_identity.starts_with("dropbox:"));
#if defined(__APPLE__) || defined(_WIN32)
  REQUIRE(moved.metadata.revision_token.starts_with("dropbox:sha256:"));
  CHECK(provider.inspect_for_undo(path_text(destination), moved.metadata.revision_token).content_matches == true);
  write_file(destination, "changed");
  CHECK(provider.inspect_for_undo(path_text(destination), moved.metadata.revision_token).content_matches == false);
  write_file(destination, "fixture");
#endif
  CHECK(provider.undo_move(path_text(source), path_text(destination), {path_text(destination.parent_path())}).success);
  CHECK(std::filesystem::exists(source));
  CHECK_FALSE(std::filesystem::exists(destination));
}

#ifdef __APPLE__
TEST_CASE("Dropbox macOS detection verifies the nearest File Provider domain", "[dropbox]") {
  namespace fs = std::filesystem;
  QTemporaryDir fixture;
  REQUIRE(fixture.isValid());
  const auto base = fs::canonical(one_drive::path_from_qstring(fixture.path()));
  const auto root = base / "Relocated Dropbox";
  const auto nearest = root / "Separate Domain";
  const auto malformed = base / "Unverified Dropbox";
  const auto link = base / "Shortcut";
  fs::create_directories(nearest);
  fs::create_directories(malformed);
  fs::create_directory_symlink(root, link);

  const std::map<fs::path, QByteArray> domains{
      {root, "com.getdropbox.dropbox.fileprovider/account"},
      {nearest, "com.getdropbox.dropbox.fileprovider.other/account"},
      {malformed, "com.getdropbox.dropbox.fileprovider/"},
  };
  const auto reader = [&](const fs::path& path, std::error_code&) -> std::optional<QByteArray> {
    const auto it = domains.find(path);
    return it == domains.end() ? std::nullopt : std::optional{it->second};
  };
  const auto verified = one_drive::detect_macos_sync_root(link, SyncProvider::Dropbox, reader);
  CHECK(verified.matched);
  CHECK(verified.authoritative);
  CHECK_FALSE(verified.message.contains(QStringLiteral("account")));
  CHECK(one_drive::detect_macos_sync_root(nearest, SyncProvider::Dropbox, reader).vetoed);
  CHECK_FALSE(one_drive::detect_macos_sync_root(malformed, SyncProvider::Dropbox, reader).matched);
  CHECK(one_drive::detect_macos_sync_root(root, SyncProvider::OneDrive, reader).vetoed);
}
#endif

#ifdef _WIN32

class TestHandle {
public:
  explicit TestHandle(HANDLE value) : value(value) {
    require(value != INVALID_HANDLE_VALUE, "could not open Windows fixture file");
  }
  TestHandle(const TestHandle &) = delete;
  TestHandle &operator=(const TestHandle &) = delete;
  ~TestHandle() { CloseHandle(value); }

  HANDLE value;
};

TEST_CASE("OneDrive content revision survives file replacement", "[onedrive]") {
  using namespace std::chrono_literals;
  QTemporaryDir temp;
  require(temp.isValid(), "could not create content revision fixture");
  const auto root = one_drive::path_from_qstring(temp.path());
  const auto source = root / "source.txt";
  const auto destination = root / "destination.txt";
  write_file(source, "original");
  const NativeSyncStorageProvider provider(SyncProvider::OneDrive);
  const auto moved = provider.move_entry(path_text(source), path_text(destination));
  REQUIRE(moved.success);
  const auto &metadata = moved.metadata;
  const auto native = one_drive::inspect_onedrive_path(destination).state;
  const auto &fingerprint = metadata.revision_token;
  REQUIRE(fingerprint.starts_with("onedrive:sha256:"));

  const auto replacement = root / "replacement.txt";
  const auto new_time = std::filesystem::last_write_time(destination) + 5s;
  write_file(replacement, "original");
  std::filesystem::last_write_time(replacement, new_time);
  std::filesystem::remove(destination);
  std::filesystem::rename(replacement, destination);
  require(one_drive::local_content_revision(destination, native.stable_identity,
                                            native.revision_suffix).isEmpty(),
          "captured contents from a file whose native metadata changed after the move");

  const auto unchanged = provider.inspect_for_undo(path_text(destination), fingerprint);
  require(unchanged.content_matches == true && unchanged.status.stable_identity != metadata.stable_identity,
          "unchanged contents did not verify after Windows metadata changed");

  write_file(destination, "modified");
  std::filesystem::last_write_time(destination, new_time);
  REQUIRE(provider.inspect_for_undo(path_text(destination), fingerprint).content_matches == false);
  const TestHandle lock(CreateFileW(destination.c_str(), GENERIC_READ, 0, nullptr,
                                   OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
  const auto unavailable = provider.inspect_for_undo(path_text(destination), fingerprint);
  require(unavailable.content_matches == false && unavailable.status.message.find("verify") != std::string::npos,
          "exclusive lock did not fail the content check as unavailable");
}

TEST_CASE("OneDrive exclusive windows lock can be retried", "[onedrive]") {
  QTemporaryDir temp;
  require(temp.isValid(), "could not create lock fixture");
  const auto root = one_drive::path_from_qstring(temp.path());
  const auto source = root / "source.txt";
  const auto destination = root / "destination.txt";
  write_file(source, "fixture");
  const NativeSyncStorageProvider provider(SyncProvider::OneDrive);
  const auto from = path_text(source);
  const auto to = path_text(destination);
  {
    const TestHandle lock(CreateFileW(source.c_str(), GENERIC_READ, 0, nullptr,
                                     OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    const auto preflight = provider.preflight_move(from, to);
    require(!preflight.allowed && preflight.sync_locked && preflight.should_retry && preflight.retry_after_ms > 0,
            "preflight failed to detect an exclusive Windows lock");

    // Hide the lock from preflight to exercise the same error path as a lock taken after preflight.
    const FakePlatformProbe probe;
    const auto failed = NativeSyncStorageProvider(SyncProvider::OneDrive, false, probe).move_entry(from, to);
    require(!failed.success && failed.skipped && failed.message == one_drive::move_lock_message().toStdString(),
            "rename sharing violation was not classified as retryable");
    require(std::filesystem::exists(source) && !std::filesystem::exists(destination),
            "failed move changed fixture files");
  }
  REQUIRE(provider.preflight_move(from, to).allowed);
  REQUIRE(provider.move_entry(from, to).success);
}

class CloudFixtureRoot {
public:
  explicit CloudFixtureRoot(const std::filesystem::path &path,
                            const wchar_t* provider = L"AIFS synthetic OneDrive fixture") : path(path) {
    CF_SYNC_REGISTRATION registration{};
    registration.StructSize = sizeof(registration);
    registration.ProviderName = provider;
    registration.ProviderVersion = L"1.0";
    CF_SYNC_POLICIES policies{};
    policies.StructSize = sizeof(policies);
    policies.Hydration.Primary = CF_HYDRATION_POLICY_FULL;
    policies.Population.Primary = CF_POPULATION_POLICY_ALWAYS_FULL;
    policies.InSync = CF_INSYNC_POLICY_TRACK_FILE_LAST_WRITE_TIME;
    const HRESULT result = CfRegisterSyncRoot(path.c_str(), &registration, &policies,
                                              CF_REGISTER_FLAG_NONE);
    if (FAILED(result)) {
      fail("could not register Cloud Files fixture: " + std::to_string(result));
    }
  }
  CloudFixtureRoot(const CloudFixtureRoot &) = delete;
  CloudFixtureRoot &operator=(const CloudFixtureRoot &) = delete;
  ~CloudFixtureRoot() { CfUnregisterSyncRoot(path.c_str()); }

private:
  std::filesystem::path path;
};

TEST_CASE("Dropbox Windows detection uses Cloud Files registration", "[dropbox]") {
  QTemporaryDir fixture;
  REQUIRE(fixture.isValid());
  const auto root = one_drive::path_from_qstring(fixture.path()) / "Documents";
  std::filesystem::create_directory(root);
  const CloudFixtureRoot cloud_root(root, L"Dropbox");
  const NativeSyncStorageProvider dropbox(SyncProvider::Dropbox);
  const NativeSyncStorageProvider onedrive(SyncProvider::OneDrive);
  const auto detection = dropbox.detect(path_text(root));
  CHECK(detection.matched);
  CHECK(detection.detection_source == "windows_cloud_files");
  CHECK_FALSE(onedrive.detect(path_text(root)).matched);
}

TestHandle open_placeholder(const std::filesystem::path &path) {
  return TestHandle(CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE,
                                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
}

void set_in_sync(const std::filesystem::path &path, CF_IN_SYNC_STATE state) {
  const auto handle = open_placeholder(path);
  const auto result = CfSetInSyncState(handle.value, state, CF_SET_IN_SYNC_FLAG_NONE, nullptr);
  if (FAILED(result)) {
    fail("could not change fixture's in-sync state: " + std::to_string(result));
  }
}

TEST_CASE("OneDrive cloud revision tracks writes instead of sync state", "[onedrive]") {
  using namespace std::chrono_literals;
  QTemporaryDir temp;
  require(temp.isValid(), "could not create Cloud Files fixture");
  const auto root = one_drive::path_from_qstring(temp.path());
  const CloudFixtureRoot cloud_root(root);
  const auto source = root / "source.txt";
  write_file(source, "original");
  const auto whole_second = std::chrono::floor<std::chrono::seconds>(
      std::filesystem::last_write_time(source));
  std::filesystem::last_write_time(source, whole_second + 10ms);
  const NativeSyncStorageProvider provider(SyncProvider::OneDrive);
  const auto local = provider.inspect_path(path_text(source));
  {
    const auto handle = open_placeholder(source);
    constexpr char identity[] = "aifs-test-item";
    require(SUCCEEDED(CfConvertToPlaceholder(handle.value, identity, sizeof(identity) - 1,
                                            CF_CONVERT_FLAG_MARK_IN_SYNC, nullptr, nullptr)),
            "could not convert fixture to Cloud Files placeholder");
  }
  const auto saved = provider.inspect_path(path_text(source));
  require(!saved.revision_token.empty() && saved.stable_identity == local.stable_identity &&
              saved.revision_token == local.revision_token,
          "placeholder conversion changed a local file's identity or revision");
  REQUIRE(scan(root, kFiles | kRecursive).size() == 1);

  set_in_sync(source, CF_IN_SYNC_STATE_NOT_IN_SYNC);
  {
    const auto handle = open_placeholder(source);
    require(SUCCEEDED(CfSetPinState(handle.value, CF_PIN_STATE_PINNED, CF_SET_PIN_FLAG_NONE, nullptr)),
            "could not pin Cloud Files fixture");
  }
  const auto dirty = provider.inspect_path(path_text(source));
  require(dirty.revision_token == saved.revision_token,
          "pin or sync state changed the unchanged placeholder revision");

  {
    std::fstream file(source, std::ios::in | std::ios::out | std::ios::binary);
    file << "modified";
    require(file.good(), "could not edit fixture's content");
  }
  // Use the same whole second as the original so only a full-resolution revision can detect the edit.
  std::filesystem::last_write_time(source, whole_second + 20ms);
  set_in_sync(source, CF_IN_SYNC_STATE_IN_SYNC);
  const auto edited = provider.inspect_path(path_text(source));
  require(edited.stable_identity == saved.stable_identity && edited.revision_token != saved.revision_token,
          "same-length edit did not change the synchronized placeholder revision");
}

#endif

} // namespace
