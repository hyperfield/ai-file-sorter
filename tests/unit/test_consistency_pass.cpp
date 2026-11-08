#include "ConsistencyPassService.hpp"
#include "ILLMClient.hpp"
#include "Settings.hpp"
#include "TestHelpers.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <catch2/catch_test_macros.hpp>
#include <atomic>
#include <functional>

namespace {
class ConsistencyReply final : public ILLMClient {
public:
    explicit ConsistencyReply(std::function<std::string(const std::string&)> callback)
        : reply(std::move(callback)) {}
    std::string categorize_file(const std::string&, const std::string&, FileType, const std::string&) override { return {}; }
    std::string complete_prompt(const std::string& prompt, int) override { return reply(prompt); }
    void set_prompt_logging_enabled(bool) override {}
    std::function<std::string(const std::string&)> reply;
};
}

TEST_CASE("Consistency pass maps opaque IDs back to same-named files without exposing source paths", "[prompt-contract][consistency]")
{
    for (bool json : {false, true}) {
        TempDir directory;
        DatabaseManager database(directory.path().string());
        auto resolved = database.resolve_category("Documents", "Reports");
        CategorizedFile first{"/private/source_alpha", "same.pdf", FileType::File,
                             resolved.category, resolved.subcategory, resolved.taxonomy_id};
        CategorizedFile second = first;
        second.file_path = "/private/source_beta";
        std::vector<CategorizedFile> rows{first, second}, newly{first, second};
        ConsistencyPassService service(database, nullptr);
        std::atomic<bool> stop{false};
        int requests = 0;
        service.run(rows, newly, [&] {
            return std::make_unique<ConsistencyReply>([&](const std::string& prompt) {
                ++requests;
                CHECK(prompt.find(first.file_path) == std::string::npos);
                CHECK(prompt.find(second.file_path) == std::string::npos);
                CHECK(prompt.find("/home/user/") == std::string::npos);
                auto matches = QRegularExpression("- id: (item-[0-9a-f]+)")
                    .globalMatch(QString::fromStdString(prompt));
                QStringList ids;
                while (matches.hasNext()) ids << matches.next().captured(1);
                REQUIRE(ids.size() == 2);
                REQUIRE(ids[0] != ids[1]);
                if (json) {
                    return QJsonDocument(QJsonArray{
                        QJsonObject{{"id", ids[0]}, {"category", "Documents"}, {"subcategory", "Invoices"}},
                        QJsonObject{{"id", ids[1]}, {"category", "Images"}, {"subcategory", "Trips"}}
                    }).toJson(QJsonDocument::Compact).toStdString();
                }
                return ids[0].toStdString() + " => Documents : Invoices\n" +
                    ids[1].toStdString() + " => Images : Trips\nEND";
            });
        }, stop, CategoryLanguage::English, {});
        CHECK(requests == 1);
        CHECK(rows[0].canonical_category == "Documents");
        CHECK(rows[0].canonical_subcategory == "Invoices");
        CHECK(rows[1].canonical_category == "Images");
        CHECK(rows[1].canonical_subcategory == "Trips");
        CHECK(newly[0].canonical_subcategory == "Invoices");
        CHECK(newly[1].canonical_subcategory == "Trips");
    }
}

TEST_CASE("Unconstrained consistency harmonization preserves active whitelist assignments", "[prompt-contract][consistency]")
{
    TempDir directory;
    EnvVarGuard guard("AI_FILE_SORTER_CONFIG_DIR", directory.path().string());
    Settings settings;
    settings.set_use_whitelist(true);
    settings.set_allowed_categories({"Documents"});
    DatabaseManager database(settings.get_config_dir());
    const auto resolved = database.resolve_literal_category("Documents", "Other");
    CategorizedFile row{"/private/source", "same.pdf", FileType::File,
                        resolved.category, resolved.subcategory, resolved.taxonomy_id};
    std::vector<CategorizedFile> rows{row}, newly{row};
    ConsistencyPassService service(database, nullptr, &settings);
    std::atomic<bool> stop{false};
    int requests = 0;
    service.run(rows, newly, [&] {
        ++requests;
        return std::make_unique<ConsistencyReply>([](const auto&) { return "END"; });
    }, stop, CategoryLanguage::English, {});
    CHECK(requests == 0);
    CHECK(rows[0].category == "Documents");
    CHECK(rows[0].subcategory == "Other");
}
