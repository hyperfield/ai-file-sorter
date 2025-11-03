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

#include <filesystem>
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

    Json::Value items(Json::arrayValue);
    for (const auto* item : chunk) {
        if (!item) {
            continue;
        }
        Json::Value obj;
        obj["id"] = make_item_key(*item);
        obj["file"] = item->file_name;
        obj["category"] = item->category;
        obj["subcategory"] = item->subcategory;
        items.append(obj);
    }

    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    const std::string taxonomy_str = Json::writeString(builder, taxonomy_json);
    const std::string items_str = Json::writeString(builder, items);

    std::ostringstream prompt;
    prompt << "You are a taxonomy normalization assistant.\n";
    prompt << "Your task is to review existing (category, subcategory) assignments for files and make them consistent.\n";
    prompt << "Guidelines:\n";
    prompt << "1. Prefer using the known taxonomy entries when they closely match.\n";
    prompt << "2. Merge near-duplicate labels (e.g. 'Docs' vs 'Documents'), but do not collapse distinct concepts.\n";
    prompt << "3. Preserve the intent of each file. If a category/subcategory already looks appropriate, keep it.\n";
    prompt << "4. Always provide both category and subcategory strings. If a subcategory is not needed, repeat the category.\n";
    prompt << "5. Output JSON only, no prose.\n\n";
    prompt << "Known taxonomy entries (JSON array): " << taxonomy_str << "\n";
    prompt << "Items to harmonize (JSON array): " << items_str << "\n";
    prompt << "Return a JSON object with the following structure: {\"harmonized\": [{\"id\": string, \"category\": string, \"subcategory\": string}, ...]}";
    prompt << "\nMatch each output object to the corresponding input by id and keep the order identical.";

    return prompt.str();
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
    return nullptr;
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

} // namespace

ConsistencyPassService::ConsistencyPassService(DatabaseManager& db_manager,
                                               std::shared_ptr<spdlog::logger> logger)
    : db_manager(db_manager),
      logger(std::move(logger))
{
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
        try {
            const std::string response = llm->complete_prompt(prompt, 512);

            Json::Value root;
            if (const Json::Value* harmonized = parse_consistency_response(response, root, logger)) {
                for (const auto& entry : *harmonized) {
                    if (auto update = extract_harmonized_update(entry, items_by_key, logger)) {
                        apply_harmonized_update(*update, db_manager, new_items_by_key, progress_callback, logger);
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
