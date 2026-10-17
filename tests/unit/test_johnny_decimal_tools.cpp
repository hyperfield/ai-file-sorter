#include <catch2/catch_test_macros.hpp>

#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QSyntaxHighlighter>
#include <QTabWidget>
#include <QTextBlock>
#include <QTextLayout>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QUrl>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>

#include "FolderStructureInitializerDialog.hpp"
#include "JohnnyDecimalArchiveIndex.hpp"
#include "JohnnyDecimalIndexWidget.hpp"
#include "JohnnyDecimalIndexExport.hpp"
#include "JohnnyDecimalItemAnalysis.hpp"
#include "JohnnyDecimalValidator.hpp"
#include "JohnnyDecimalValidationHighlighter.hpp"
#include "TestHelpers.hpp"
#include "Utils.hpp"

namespace {

constexpr int kIndexTab = 2;
constexpr int kValidationTab = 3;

void make_archive(const std::filesystem::path& root) {
    for (const auto* path : {"20-29 Work/21 Projects/21.03 Alpha/Drafts/2026-10-16 Notes",
                            "20-29 Work/21 Projects/21.03 Beta", "20-29 Work/21 Projects/21.4 Broken",
                            "20-29 Work/21 Projects/22.01 Wrong", "20-29 Work/21 Projects/Loose notes",
                            "20-29 Work/22 Reference/21.03 Copy", "20-29 Work/22 Reference/22.00 Index"}) {
        std::filesystem::create_directories(root / path);
    }
}

QTreeWidgetItem* row_for(QTreeWidget* tree, const QString& path) {
    for (QTreeWidgetItemIterator it(tree); *it; ++it) {
        if ((*it)->data(0, Qt::UserRole).toString() == path) {
            return *it;
        }
    }
    return nullptr;
}

FolderStructurePluginProfile jd_profile() {
    FolderStructurePluginProfile profile;
    profile.id = "test-jd";
    profile.name = "Test Johnny.Decimal";
    profile.description = "Test structure profile";
    profile.structure_kind = "johnny_decimal";
    profile.initial_directories = {"20-29 Work/21 Projects"};
    return profile;
}

}  // namespace

TEST_CASE("JohnnyDecimalItemAnalysis parses standard IDs without truncating malformed numbers") {
    using JohnnyDecimalItemAnalysis::parse_name;
    CHECK(parse_name("00.00 Index").valid);
    CHECK(parse_name("99.99 Last").valid);
    const auto name = parse_name(" 21.03 Project name ");
    CHECK(name.valid);
    CHECK(name.id == "21.03");
    CHECK(name.label == "Project name");
    for (const auto* malformed : {"21.3 Short", "2.03 Short", "021.03 Long", "21.003 Long",
                                   "21_03 Separator", "21.03NoSpace", "21.03", "21.03.01 Extra",
                                   "999999999999999999999999999999.03 Huge"}) {
        CAPTURE(malformed);
        CHECK_FALSE(parse_name(malformed).valid);
        CHECK(parse_name(malformed).numbered);
    }
    CHECK_FALSE(parse_name("Notes").numbered);
    CHECK_FALSE(parse_name("").valid);
}

TEST_CASE("JohnnyDecimalValidator checks direct item IDs only when requested") {
    TempDir root;
    make_archive(root.path());
    const auto basic = JohnnyDecimalValidator::validate_archive(root.path());
    CHECK(basic.issues.empty());
    CHECK_FALSE(basic.item_ids_checked);

    const auto report = JohnnyDecimalValidator::validate_archive(root.path(), JohnnyDecimalValidator::Scope::Items);
    CHECK(report.item_ids_checked);
    CHECK(report.item_count == 7);
    std::multiset<std::string> codes;
    for (const auto& issue : report.issues) {
        codes.insert(issue.code);
        CHECK(issue.relative_path.find("Drafts") == std::string::npos);
        CHECK_FALSE(issue.suggestion.empty());
    }
    CHECK(codes.count("duplicate_item_id") == 3);
    CHECK(codes.count("item_category_mismatch") == 2);
    CHECK(codes.count("item_id_malformed") == 1);
    CHECK(codes.count("missing_item_id") == 1);
    CHECK(report.has_errors());
    CHECK(JohnnyDecimalValidator::format_report(report).find("Item folders checked: 7") != std::string::npos);
}

TEST_CASE("JohnnyDecimalValidator accepts item boundary IDs and preserves scan warnings") {
    TempDir root;
    std::filesystem::create_directories(root.path() / "00-09 System/00 Index/00.00 Master/Notes");
    std::filesystem::create_directories(root.path() / "90-99 Archive/99 Reference/99.99 Last");
    const auto clean = JohnnyDecimalValidator::validate_archive(root.path(), JohnnyDecimalValidator::Scope::Items);
    CHECK(clean.issues.empty());
    CHECK(clean.item_count == 2);
    const auto limited = JohnnyDecimalValidator::validate_archive(root.path(), JohnnyDecimalValidator::Scope::Items, 8, 2);
    CHECK(std::any_of(limited.issues.begin(), limited.issues.end(), [](const auto& issue) {
        return issue.code == "scan_limit_reached";
    }));
    CHECK_FALSE(JohnnyDecimalValidator::validate_archive(root.path() / "absent",
        JohnnyDecimalValidator::Scope::Items).scanned);
}

TEST_CASE("JohnnyDecimalValidationHighlighter styles severities and preserves report text") {
    QtAppContext qt;
    QPlainTextEdit editor;
    QPalette palette = editor.palette();
    palette.setColor(QPalette::Base, Qt::white);
    palette.setColor(QPalette::Text, Qt::black);
    editor.setPalette(palette);
    JohnnyDecimalValidationHighlighter highlighter(&editor);
    JohnnyDecimalValidator::Report report;
    report.root = "/archive/<Family> & work";
    report.issues = {
        {JohnnyDecimalValidator::Severity::Error, "duplicate_item_id", "Duplicate item ID",
         "21 Projects/21.01 <Research> & notes", "Duplicate number.", "Choose a unique ID."},
        {JohnnyDecimalValidator::Severity::Warning, "missing_item_id", "Missing item ID",
         "21 Projects/Meeting notes", "No item number.", "Add an ID."},
        {JohnnyDecimalValidator::Severity::Info, "scan_note", "Archive note", "", "Scan details.", ""}};
    const auto text = QString::fromStdString(JohnnyDecimalValidator::format_report(report));
    editor.setPlainText(text);
    highlighter.rehighlight();
    auto format_at = [&editor](const QString& prefix, int offset = 0) {
        for (auto block = editor.document()->firstBlock(); block.isValid(); block = block.next()) {
            if (!block.text().startsWith(prefix)) {
                continue;
            }
            for (const auto& range : block.layout()->formats()) {
                if (range.start <= offset && offset < range.start + range.length) {
                    return range.format;
                }
            }
        }
        return QTextCharFormat{};
    };
    const auto error = format_at(QStringLiteral("ERROR -"));
    CHECK(error.fontWeight() == QFont::Bold);
    CHECK(error.foreground().color() == QColor(QStringLiteral("#c62828")));
    const auto warning = format_at(QStringLiteral("WARNING -"));
    CHECK(warning.fontWeight() == QFont::Bold);
    CHECK(warning.foreground().color() == QColor(QStringLiteral("#ffdf66")));
    CHECK(warning.background().color() == QColor(QStringLiteral("#493900")));
    CHECK(format_at(QStringLiteral("INFO -")).foreground().color() == QColor(QStringLiteral("#1565c0")));
    CHECK(format_at(QStringLiteral("Path:")).fontWeight() == QFont::Bold);
    CHECK(format_at(QStringLiteral("Suggested fix:")).fontWeight() == QFont::Bold);
    CHECK(format_at(QStringLiteral("Johnny.Decimal validation report")).fontWeight() == QFont::Bold);
    CHECK(editor.toPlainText() == text);

    palette.setColor(QPalette::Base, QColor(QStringLiteral("#202020")));
    palette.setColor(QPalette::Text, Qt::white);
    editor.setPalette(palette);
    CHECK(format_at(QStringLiteral("ERROR -")).foreground().color() == QColor(QStringLiteral("#ff8a80")));
    CHECK(format_at(QStringLiteral("WARNING -")).foreground() == warning.foreground());
    CHECK(editor.toPlainText() == text);
    editor.clear();
    CHECK(editor.toPlainText().isEmpty());
}

TEST_CASE("JohnnyDecimalArchiveIndex exports complete escaped local folder links") {
    TempDir root;
    const std::string item = "20-29 Work/21 Projects/21.03 [Alpha] #1 (draft)";
    std::filesystem::create_directories(root.path() / item / "Notes");
    std::filesystem::create_directories(root.path() / "Loose");
    const auto unicode_path = Utils::utf8_to_path("20-29 Work/21 Projects/21.04 \xC3\x98konomi");
    std::filesystem::create_directories(root.path() / unicode_path);
    auto index = JohnnyDecimalArchiveIndex::build_index(root.path());
    const auto markdown = JohnnyDecimalArchiveIndex::format_markdown(index);
    CHECK(markdown.find("21.03 \\[Alpha\\] \\#1 (draft)") != std::string::npos);
    CHECK(markdown.find("[Notes](<file://") != std::string::npos);
    CHECK(markdown.find("[Loose](<file://") != std::string::npos);
    const auto target = QUrl::fromLocalFile(QString::fromStdString((root.path() / item).string()));
    CHECK(markdown.find(target.toEncoded().toStdString()) != std::string::npos);
    const auto unicode_url = QUrl::fromLocalFile(QString::fromStdString(Utils::path_to_utf8(root.path() / unicode_path)));
    CHECK(markdown.find(unicode_url.toEncoded().toStdString()) != std::string::npos);
    index.scan_limit_reached = true;
    CHECK(JohnnyDecimalArchiveIndex::format_markdown(index).find("incomplete") != std::string::npos);
}

TEST_CASE("JohnnyDecimalIndexExport saves complete Markdown and reports filesystem errors") {
    TempDir root;
    TempDir output;
    make_archive(root.path());
    const auto index = JohnnyDecimalArchiveIndex::build_index(root.path());
    const auto destination = output.path() / "archive-index.md";
    std::string error;
    REQUIRE(JohnnyDecimalIndexExport::save(index, destination, &error));
    std::ifstream input(destination, std::ios::binary);
    const std::string content(std::istreambuf_iterator<char>(input), {});
    CHECK(content == JohnnyDecimalArchiveIndex::format_markdown(index));
    CHECK(error.empty());
    CHECK_FALSE(JohnnyDecimalIndexExport::save(index, output.path() / "absent/index.md", &error));
    CHECK_FALSE(error.empty());
}

TEST_CASE("JohnnyDecimalIndexWidget filters conflicts and opens only existing archive folders") {
    QtAppContext qt;
    TempDir root;
    make_archive(root.path());
    std::vector<QUrl> opened;
    JohnnyDecimalIndexWidget widget(nullptr, [&opened](const QUrl& url) {
        opened.push_back(url);
        return true;
    });
    widget.set_index(JohnnyDecimalArchiveIndex::build_index(root.path()));
    auto* tree = widget.findChild<QTreeWidget*>();
    auto* search = widget.findChild<QLineEdit*>();
    auto* open = widget.findChild<QToolButton*>(QStringLiteral("johnnyDecimalIndexOpen"));
    REQUIRE(tree);
    REQUIRE(search);
    REQUIRE(open);
    auto* alpha = row_for(tree, QStringLiteral("20-29 Work/21 Projects/21.03 Alpha"));
    auto* copy = row_for(tree, QStringLiteral("20-29 Work/22 Reference/21.03 Copy"));
    REQUIRE(alpha);
    REQUIRE(copy);
    CHECK(alpha->text(2).contains(QStringLiteral("Duplicate item ID")));
    CHECK(copy->text(2).contains(QStringLiteral("Item in wrong category")));
    search->setText(QStringLiteral("21.03"));
    CHECK_FALSE(alpha->isHidden());
    CHECK_FALSE(copy->isHidden());
    CHECK_FALSE(copy->parent()->isHidden());
    search->setText(QStringLiteral("alpha"));
    CHECK_FALSE(alpha->isHidden());
    CHECK(copy->isHidden());
    tree->setCurrentItem(alpha);
    REQUIRE(open->isEnabled());
    open->click();
    REQUIRE(opened.size() == 1);
    CHECK(opened.front().toLocalFile().toStdString() == std::filesystem::canonical(
        root.path() / "20-29 Work/21 Projects/21.03 Alpha").string());
    std::filesystem::remove_all(root.path() / "20-29 Work/21 Projects/21.03 Alpha");
    open->click();
    CHECK(opened.size() == 1);
    TempDir external;
    std::error_code link_error;
    std::filesystem::create_directory_symlink(external.path(),
        root.path() / "20-29 Work/21 Projects/21.03 Alpha", link_error);
    if (!link_error) {
        open->click();
        CHECK(opened.size() == 1);
    }
    search->setText(QStringLiteral("no matches"));
    CHECK_FALSE(open->isEnabled());
    widget.set_index({});
    CHECK(tree->topLevelItemCount() == 0);
    CHECK_FALSE(widget.findChild<QToolButton*>(QStringLiteral("johnnyDecimalIndexExport"))->isEnabled());
}

TEST_CASE("FolderStructureInitializerDialog enhances tools only for Johnny Decimal profiles") {
    QtAppContext qt;
    TempDir root;
    make_archive(root.path());
    SECTION("No plugin profile") {
        FolderStructureInitializerDialog dialog(root.path());
        CHECK(dialog.findChild<QWidget*>(QStringLiteral("johnnyDecimalIndexWidget")) == nullptr);
        CHECK(dialog.findChild<QPlainTextEdit*>(QStringLiteral("johnnyDecimalArchiveIndexEdit")) != nullptr);
        dialog.findChild<QTabWidget*>()->setCurrentIndex(kValidationTab);
        REQUIRE(dialog.findChild<QSyntaxHighlighter*>(QStringLiteral("johnnyDecimalValidationHighlighter")));
        CHECK_FALSE(dialog.findChild<QPlainTextEdit*>(QStringLiteral("johnnyDecimalValidationReportEdit"))
                        ->toPlainText().contains(QStringLiteral("Duplicate item ID")));
    }
    SECTION("Enabled Johnny Decimal profile") {
        FolderStructureInitializerDialog dialog(root.path(), {jd_profile()});
        REQUIRE(dialog.findChild<QWidget*>(QStringLiteral("johnnyDecimalIndexWidget")));
        dialog.findChild<QTabWidget*>()->setCurrentIndex(kIndexTab);
        CHECK(dialog.findChild<QTreeWidget*>()->topLevelItemCount() == 1);
        dialog.findChild<QTabWidget*>()->setCurrentIndex(kValidationTab);
        CHECK(dialog.findChild<QPlainTextEdit*>(QStringLiteral("johnnyDecimalValidationReportEdit"))
                  ->toPlainText().contains(QStringLiteral("Duplicate item ID")));
        auto* destination = dialog.findChild<QLineEdit*>(QStringLiteral("folderStructureDestinationEdit"));
        destination->clear();
        dialog.findChild<QTabWidget*>()->setCurrentIndex(kIndexTab);
        CHECK(dialog.findChild<QTreeWidget*>()->topLevelItemCount() == 0);
    }
    SECTION("Other plugin profile") {
        auto profile = jd_profile();
        profile.structure_kind = "numbered_prefix";
        FolderStructureInitializerDialog dialog(root.path(), {profile});
        CHECK(dialog.findChild<QWidget*>(QStringLiteral("johnnyDecimalIndexWidget")) == nullptr);
    }
}
