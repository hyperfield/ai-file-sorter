#include "ConsistencyPassService.hpp"

#include "ILLMClient.hpp"

#include <fmt/format.h>

#include <spdlog/spdlog.h>

#ifdef _WIN32
#include <json/json.h>
#elif __APPLE__
#include <json/json.h>
#else
#include <jsoncpp/json/json.h>
#endif

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <optional>
#include <sstream>

namespace {

std::string trim_whitespace(const std::string& value) {
    const char* whitespace = " \t\n\r\f\v";
    const auto start = value.find_first_not_of(whitespace);
    const auto end = value.find_last_not_of(whitespace);
    if (start == std::string::npos || end == std::string::npos) {
        return std::string();
    }
    return value.substr(start, end - start + 1);
}

std::string make_item_key(const CategorizedFile& item) {
    std::filesystem::path path(item.file_path);
    path /= item.file_name;
    return path.generic_string();
}

std::string build_consistency_prompt(
    const std::vector<const CategorizedFile*>& chunk,
    const std::vector<std::pair<std::string, std::string>>& taxonomy)
{
    Json::Value taxonomy_json(Json::arrayValue);
    for (const auto& entry : taxonomy) {
        Json::Value obj;
        obj["category"] = entry.first;
        obj["subcategory"] = entry.second;
        taxonomy_json.append(obj);
    }

    std::ostringstream prompt;
    prompt << "You are a taxonomy normalization assistant.\n";
    prompt << "Your task is to review existing (category, subcategory) assignments for files and make them consistent.\n";
    prompt << "Guidelines:\n";
    prompt << "1. Prefer using the known taxonomy entries when they closely match.\n";
    prompt << "2. Merge near-duplicate labels (e.g. 'Docs' vs 'Documents'), but do not collapse distinct concepts.\n";
    prompt << "3. Preserve the intent of each file. If a category/subcategory already looks appropriate, keep it.\n";
    prompt << "4. Always provide both category and subcategory strings.\n";
    prompt << "5. Respond with one line per item using the exact format: <id> => <Category> : <Subcategory>.\n";
    prompt << "6. The <id> must be copied verbatim from the list below (full path). No other text may appear before it.\n";
    prompt << "7. Keep the output order identical to the input and finish by writing END on its own line. No other prose.\n\n";

    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    const std::string taxonomy_str = Json::writeString(builder, taxonomy_json);
    prompt << "Known taxonomy entries (JSON array): " << taxonomy_str << "\n\n";

    prompt << "Items to harmonize (follow the input order in your response):\n";
    for (const auto* item : chunk) {
        if (!item) {
            continue;
        }
        prompt << "- id: " << make_item_key(*item)
               << ", file: " << item->file_name
               << ", current: " << item->category << " / " << item->subcategory << "\n";
    }

    prompt << "Example response lines:\n";
    prompt << "/home/user/Downloads/setup.exe => Applications : Installers\n";
    prompt << "/home/user/Documents/taxes.pdf => Documents : Tax forms\n";
    prompt << "END";

    return prompt.str();
}

bool parse_structured_lines(
    const std::string& response,
    Json::Value& root,
    const std::shared_ptr<spdlog::logger>& logger)
{
    Json::Value harmonized(Json::arrayValue);
    std::istringstream stream(response);
    std::string raw_line;
    size_t line_number = 0;

    while (std::getline(stream, raw_line)) {
        ++line_number;
        std::string line = trim_whitespace(raw_line);
        if (line.empty()) {
            continue;
        }
        if (line == "END") {
            break;
        }

        const auto arrow_pos = line.find("=>");
        if (arrow_pos == std::string::npos) {
            continue;
        }

        std::string id = trim_whitespace(line.substr(0, arrow_pos));
        std::string remainder = trim_whitespace(line.substr(arrow_pos + 2));
        const auto colon_pos = remainder.find(':');
        if (colon_pos == std::string::npos) {
            continue;
        }

        std::string category = trim_whitespace(remainder.substr(0, colon_pos));
        std::string subcategory = trim_whitespace(remainder.substr(colon_pos + 1));
        if (subcategory.empty()) {
            subcategory = category;
        }

        if (id.empty() || category.empty()) {
            if (logger) {
                logger->warn("Consistency pass skipped malformed line {}: '{}'", line_number, raw_line);
            }
            continue;
        }

        Json::Value obj(Json::objectValue);
        obj["id"] = id;
        obj["category"] = category;
        obj["subcategory"] = subcategory;
        harmonized.append(obj);
    }

    if (harmonized.empty()) {
        if (logger) {
            logger->warn("Consistency pass parsed zero harmonized entries from line-based response");
        }
        return false;
    }

    root = harmonized;
    return true;
}

struct HarmonizedUpdate {
    std::string id;
    CategorizedFile* target{nullptr};
    std::string category;
    std::string subcategory;
};

std::optional<HarmonizedUpdate> extract_harmonized_update(
    const Json::Value& entry,
    std::unordered_map<std::string, CategorizedFile*>& items_by_key,
    const std::shared_ptr<spdlog::logger>& logger)
{
    if (!entry.isObject()) {
        return std::nullopt;
    }

    const std::string id = entry.get("id", "").asString();
    if (id.empty()) {
        return std::nullopt;
    }

    auto it = items_by_key.find(id);
    if (it == items_by_key.end() || !it->second) {
        if (logger) {
            logger->warn("Consistency pass referenced unknown item id '{}'", id);
        }
        return std::nullopt;
    }

    auto trim_or_fallback = [](const Json::Value& parent,
                               const char* key,
                               const std::string& fallback) {
        if (!parent.isMember(key)) {
            return fallback;
        }
        std::string candidate = parent[key].asString();
        if (candidate.empty()) {
            return fallback;
        }
        candidate = trim_whitespace(std::move(candidate));
        return candidate.empty() ? fallback : candidate;
    };

    CategorizedFile* target = it->second;
    std::string category = trim_or_fallback(entry, "category", target->category);
    if (category.empty()) {
        category = target->category;
    }

    std::string subcategory = trim_or_fallback(entry, "subcategory", target->subcategory);
    if (subcategory.empty()) {
        subcategory = category;
    }

    return HarmonizedUpdate{id, target, std::move(category), std::move(subcategory)};
}

void apply_harmonized_update(
    const HarmonizedUpdate& update,
    DatabaseManager& db_manager,
    std::unordered_map<std::string, CategorizedFile*>& new_items_by_key,
    const ConsistencyPassService::ProgressCallback& progress_callback,
    const std::shared_ptr<spdlog::logger>& logger)
{
    DatabaseManager::ResolvedCategory resolved =
        db_manager.resolve_category(update.category, update.subcategory);

    bool changed = (resolved.category != update.target->category) ||
                   (resolved.subcategory != update.target->subcategory);

    update.target->category = resolved.category;
    update.target->subcategory = resolved.subcategory;
    update.target->taxonomy_id = resolved.taxonomy_id;

    db_manager.insert_or_update_file_with_categorization(
        update.target->file_name,
        update.target->type == FileType::File ? "F" : "D",
        update.target->file_path,
        resolved);

    if (auto new_it = new_items_by_key.find(update.id); new_it != new_items_by_key.end() && new_it->second) {
        new_it->second->category = resolved.category;
        new_it->second->subcategory = resolved.subcategory;
        new_it->second->taxonomy_id = resolved.taxonomy_id;
    }

    if (changed) {
        const std::string message = fmt::format("[CONSISTENCY] {} -> {} / {}",
                                                update.target->file_name,
                                                resolved.category,
                                                resolved.subcategory);
        if (progress_callback) {
            progress_callback(message);
        }
        if (logger) {
            logger->info(message);
        }
    }
}

const Json::Value* parse_consistency_response(
    const std::string& response,
    Json::Value& root,
    const std::shared_ptr<spdlog::logger>& logger)
{
    Json::CharReaderBuilder reader;
    std::string errors;
    std::istringstream stream(response);
    if (!Json::parseFromStream(reader, stream, &root, &errors)) {
        if (logger) {
            logger->warn("Consistency pass JSON parse failed: {}", errors);
            logger->warn("Consistency pass raw response ({} chars):\n{}", response.size(), response);
        }
        if (parse_structured_lines(response, root, logger)) {
            return &root;
        }
        return nullptr;
    }

    if (root.isObject() && root.isMember("harmonized")) {
        const Json::Value& harmonized = root["harmonized"];
        if (harmonized.isArray()) {
            return &harmonized;
        }
    }

    if (root.isArray()) {
        return &root;
    }

    if (logger) {
        logger->warn("Consistency pass response missing 'harmonized' array");
    }

    if (parse_structured_lines(response, root, logger)) {
        return &root;
    }
    return nullptr;
}

std::vector<std::pair<std::string, std::string>> parse_ordered_category_lines(
    const std::string& response,
    const std::shared_ptr<spdlog::logger>& logger)
{
    std::vector<std::pair<std::string, std::string>> ordered;
    std::istringstream stream(response);
    std::string raw_line;
    size_t line_number = 0;

    while (std::getline(stream, raw_line)) {
        ++line_number;
        std::string line = trim_whitespace(raw_line);
        if (line.empty()) {
            continue;
        }
        if (line == "END") {
            break;
        }
        while (!line.empty() && (line.front() == '-' || line.front() == '*')) {
            line.erase(line.begin());
            line = trim_whitespace(line);
        }

        const auto colon_pos = line.find(':');
        if (colon_pos == std::string::npos) {
            continue;
        }

        std::string lhs = trim_whitespace(line.substr(0, colon_pos));
        std::string rhs = trim_whitespace(line.substr(colon_pos + 1));
        const auto arrow_pos = rhs.find("=>");
        if (arrow_pos != std::string::npos) {
            rhs = trim_whitespace(rhs.substr(0, arrow_pos));
        }

        std::string category;
        std::string subcategory;
        const auto slash_pos = lhs.find('/');
        if (slash_pos != std::string::npos) {
            category = trim_whitespace(lhs.substr(0, slash_pos));
            subcategory = trim_whitespace(lhs.substr(slash_pos + 1));
        } else {
            category = lhs;
        }

        if (subcategory.empty()) {
            subcategory = rhs;
        }
        if (subcategory.empty()) {
            subcategory = category;
        }

        if (category.empty()) {
            if (logger) {
                logger->warn("Consistency pass fallback skipped malformed line {}: '{}'", line_number, raw_line);
            }
            continue;
        }

        ordered.emplace_back(std::move(category), std::move(subcategory));
    }

    if (ordered.empty() && logger) {
        logger->warn("Consistency pass fallback parsing produced no entries");
    }

    return ordered;
}

bool apply_ordered_fallback(
    const std::string& response,
    const std::vector<const CategorizedFile*>& chunk,
    std::unordered_map<std::string, CategorizedFile*>& items_by_key,
    DatabaseManager& db_manager,
    std::unordered_map<std::string, CategorizedFile*>& new_items_by_key,
    const ConsistencyPassService::ProgressCallback& progress_callback,
    const std::shared_ptr<spdlog::logger>& logger)
{
    const auto ordered = parse_ordered_category_lines(response, logger);
    if (ordered.empty()) {
        return false;
    }

    const size_t limit = std::min(chunk.size(), ordered.size());
    bool applied = false;
    for (size_t index = 0; index < limit; ++index) {
        if (!chunk[index]) {
            continue;
        }
        Json::Value entry(Json::objectValue);
        entry["id"] = make_item_key(*chunk[index]);
        entry["category"] = ordered[index].first;
        entry["subcategory"] = ordered[index].second;
        if (auto update = extract_harmonized_update(entry, items_by_key, logger)) {
            apply_harmonized_update(*update, db_manager, new_items_by_key, progress_callback, logger);
            applied = true;
        }
    }

    return applied;
}


} // namespace

ConsistencyPassService::ConsistencyPassService(DatabaseManager& db_manager,
                                               std::shared_ptr<spdlog::logger> logger)
    : db_manager(db_manager),
      logger(std::move(logger))
{
}

void ConsistencyPassService::set_prompt_logging_enabled(bool enabled)
{
    prompt_logging_enabled = enabled;
}

void ConsistencyPassService::run(std::vector<CategorizedFile>& categorized_files,
                                 std::vector<CategorizedFile>& newly_categorized_files,
                                 std::function<std::unique_ptr<ILLMClient>()> llm_factory,
                                 std::atomic<bool>& stop_flag,
                                 const ProgressCallback& progress_callback) const
{
    if (stop_flag.load() || categorized_files.empty()) {
        return;
    }

    std::unique_ptr<ILLMClient> llm;
    try {
        llm = llm_factory ? llm_factory() : nullptr;
    } catch (const std::exception& ex) {
        if (logger) {
            logger->warn("Failed to create LLM client for consistency pass: {}", ex.what());
        }
        return;
    }

    if (!llm) {
        return;
    }

    const auto taxonomy = db_manager.get_taxonomy_snapshot(150);

    std::unordered_map<std::string, CategorizedFile*> items_by_key;
    items_by_key.reserve(categorized_files.size());
    for (auto& item : categorized_files) {
        items_by_key[make_item_key(item)] = &item;
    }

    std::unordered_map<std::string, CategorizedFile*> new_items_by_key;
    new_items_by_key.reserve(newly_categorized_files.size());
    for (auto& item : newly_categorized_files) {
        new_items_by_key[make_item_key(item)] = &item;
    }

    std::vector<const CategorizedFile*> chunk;
    chunk.reserve(10);

    for (size_t index = 0; index < categorized_files.size(); ++index) {
        if (stop_flag.load()) {
            break;
        }

        chunk.push_back(&categorized_files[index]);
        bool should_flush = chunk.size() == 10 || index + 1 == categorized_files.size();
        if (!should_flush) {
            continue;
        }

        if (logger) {
            logger->info("[CONSISTENCY] Processing chunk {}-{} of {}",
                         index + 1 - chunk.size() + 1,
                         index + 1,
                         categorized_files.size());
            for (const auto* item : chunk) {
                if (!item) continue;
                logger->info("  [BEFORE] {} -> {} / {}", item->file_name, item->category, item->subcategory);
            }
        }

        const std::string prompt = build_consistency_prompt(chunk, taxonomy);
        if (prompt_logging_enabled) {
            std::cout << "\n[CONSISTENCY PROMPT]\n" << prompt << "\n";
        }
        try {
            const std::string response = llm->complete_prompt(prompt, 512);
            if (prompt_logging_enabled) {
                std::cout << "[CONSISTENCY RESPONSE]\n" << response << "\n";
            }

            Json::Value root;
            if (const Json::Value* harmonized = parse_consistency_response(response, root, logger)) {
                for (const auto& entry : *harmonized) {
                    if (auto update = extract_harmonized_update(entry, items_by_key, logger)) {
                        apply_harmonized_update(*update, db_manager, new_items_by_key, progress_callback, logger);
                    }
                }
            } else {
                if (!apply_ordered_fallback(response,
                                             chunk,
                                             items_by_key,
                                             db_manager,
                                             new_items_by_key,
                                             progress_callback,
                                             logger)) {
                    if (logger) {
                        logger->warn("Consistency pass could not interpret response; skipping chunk");
                    }
                }
            }
        } catch (const std::exception& ex) {
            if (logger) {
                logger->warn("Consistency pass chunk failed: {}", ex.what());
            }
        }

        if (logger) {
            for (const auto* item : chunk) {
                if (!item) continue;
                logger->info("  [AFTER] {} -> {} / {}", item->file_name, item->category, item->subcategory);
            }
        }

        chunk.clear();
    }
}
