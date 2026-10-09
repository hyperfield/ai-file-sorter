#include "ApiImageAnalyzer.hpp"

#include "ILLMClient.hpp"
#include "LlavaImageAnalyzer.hpp"
#include "Logger.hpp"
#include "Utils.hpp"
#include "VisualModelCatalog.hpp"

#if __has_include(<jsoncpp/json/json.h>)
    #include <jsoncpp/json/json.h>
#elif __has_include(<json/json.h>)
    #include <json/json.h>
#else
    #error "jsoncpp headers not found. Install jsoncpp development files."
#endif

#include <memory>
#include <optional>
#include <stdexcept>

namespace ApiImageAnalyzer {
namespace {

// Keeps the debug line bounded; the full reply can be long when a model keeps talking.
constexpr size_t kMaxDebugContentChars = 1000;

std::string trim(const std::string& value)
{
    const char* whitespace = " \t\n\r\f\v";
    const auto start = value.find_first_not_of(whitespace);
    if (start == std::string::npos) {
        return {};
    }
    const auto end = value.find_last_not_of(whitespace);
    return value.substr(start, end - start + 1);
}

// Models often wrap JSON in a Markdown fence even when told not to. Remove one fence pair only.
std::string strip_code_fence(const std::string& text)
{
    if (text.rfind("```", 0) != 0) {
        return text;
    }
    std::string body = text;
    const auto first_line_end = body.find('\n');
    body = first_line_end == std::string::npos ? std::string() : body.substr(first_line_end + 1);
    body = trim(body);
    if (body.size() >= 3 && body.compare(body.size() - 3, 3, "```") == 0) {
        body.erase(body.size() - 3);
    }
    return trim(body);
}

// Returns the parsed root only when the whole text is one JSON object.
std::optional<Json::Value> parse_object(const std::string& text)
{
    Json::CharReaderBuilder reader_builder;
    // Trailing text after the object must fail the parse; otherwise "{...} and {...}" reads as the first object.
    reader_builder["failIfExtra"] = true;
    std::unique_ptr<Json::CharReader> reader(reader_builder.newCharReader());
    Json::Value root;
    std::string errors;
    if (!reader->parse(text.data(), text.data() + text.size(), &root, &errors) || !root.isObject()) {
        return std::nullopt;
    }
    return root;
}

// Finds the one top-level {...} object in text that may contain surrounding prose.
// Returns nothing when there are zero or several top-level objects, so ambiguous replies are rejected.
std::optional<std::string> extract_single_top_level_object(const std::string& text)
{
    std::optional<std::string> found;
    int depth = 0;
    bool in_string = false;
    bool escaped = false;
    size_t object_start = std::string::npos;
    size_t top_level_objects = 0;

    for (size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (in_string) {
            if (escaped) {
                escaped = false;
            } else if (c == '\\') {
                escaped = true;
            } else if (c == '"') {
                in_string = false;
            }
            continue;
        }
        if (c == '"') {
            in_string = true;
        } else if (c == '{') {
            if (depth == 0) {
                object_start = i;
                ++top_level_objects;
            }
            ++depth;
        } else if (c == '}' && depth > 0) {
            --depth;
            if (depth == 0) {
                found = text.substr(object_start, i - object_start + 1);
            }
        }
    }

    if (top_level_objects != 1 || depth != 0 || !found) {
        return std::nullopt;
    }
    return found;
}

void log_unparseable_reply(const std::string& content)
{
    if (auto logger = Logger::get_logger("core_logger")) {
        std::string excerpt = content;
        if (excerpt.size() > kMaxDebugContentChars) {
            excerpt.resize(kMaxDebugContentChars);
        }
        logger->warn("[VISION-DEBUG] Raw assistant content: {}", excerpt);
    }
}

} // namespace

std::optional<CustomApiEndpoint> resolve_visual_api_endpoint(std::string_view visual_model_id,
                                                              const std::vector<CustomApiEndpoint>& endpoints,
                                                              std::string* error)
{
    const auto endpoint_id = api_endpoint_id_from_visual_model_id(visual_model_id);
    if (!endpoint_id) {
        if (error) {
            *error = "Selected visual backend is not an API endpoint.";
        }
        return std::nullopt;
    }

    for (const auto& endpoint : endpoints) {
        if (endpoint.id != *endpoint_id) {
            continue;
        }
        if (!is_valid_custom_api_endpoint(endpoint)) {
            if (error) {
                *error = "Selected API endpoint is incomplete. Name, base URL and model are required.";
            }
            return std::nullopt;
        }
        if (!endpoint.supports_vision) {
            if (error) {
                *error = "Selected API endpoint is not marked as supporting image input. "
                         "Edit the endpoint and enable image support.";
            }
            return std::nullopt;
        }
        return endpoint;
    }

    if (error) {
        *error = "Selected API visual endpoint is missing. Please re-select it.";
    }
    return std::nullopt;
}

std::string build_vision_prompt()
{
    return "Describe the image for a file organizer. Reply with only a JSON object that has exactly "
           "two string keys:\n"
           "\"description\": one or two concise sentences describing the visible contents.\n"
           "\"suggested_name\": a descriptive filename of at most three lowercase words separated by "
           "underscores. Do not include a path or an extension.\n"
           "Do not guess names, places, or dates that are not visible. Do not use Markdown.";
}

std::string build_image_analysis_response_format()
{
    // Strict json_schema makes the server constrain the answer itself, so the reply is not left to
    // prompt compliance alone.
    return R"({
  "type": "json_schema",
  "json_schema": {
    "name": "image_analysis",
    "strict": true,
    "schema": {
      "type": "object",
      "properties": {
        "description": {"type": "string"},
        "suggested_name": {"type": "string"}
      },
      "required": ["description", "suggested_name"],
      "additionalProperties": false
    }
  }
})";
}

VisionReply parse_vision_reply(const std::string& content)
{
    const std::string trimmed = trim(content);

    // a. The whole reply is the JSON object.
    std::optional<Json::Value> root = parse_object(trimmed);
    // b. One Markdown fence wrapped the object; the retry is the same parse on the stripped text.
    if (!root) {
        root = parse_object(strip_code_fence(trimmed));
    }
    // c. Prose surrounds exactly one object.
    if (!root) {
        if (const auto embedded = extract_single_top_level_object(trimmed)) {
            root = parse_object(*embedded);
        }
    }
    // d. Nothing usable. Prose alone is never read as a result.
    if (!root) {
        log_unparseable_reply(content);
        throw std::runtime_error("Visual reply is malformed: no single JSON object with description and suggested_name");
    }

    VisionReply reply;
    if ((*root).isMember("description") && (*root)["description"].isString()) {
        reply.description = trim((*root)["description"].asString());
    }
    if (!(*root).isMember("suggested_name") || !(*root)["suggested_name"].isString()) {
        log_unparseable_reply(content);
        throw std::runtime_error("Visual reply is malformed: suggested_name is missing");
    }
    reply.suggested_name = trim((*root)["suggested_name"].asString());
    if (reply.suggested_name.empty()) {
        throw std::runtime_error("Visual reply has an empty suggested_name");
    }
    return reply;
}

ImageAnalysisResult analyze_image(ILLMClient& llm, const std::filesystem::path& image_path)
{
    if (!llm.supports_image_input()) {
        throw std::runtime_error("Selected LLM client does not accept image input.");
    }

    ILLMClient::ImageCompletionOptions options;
    options.max_tokens = kVisionMaxTokens;
    options.temperature = 0.0;
    options.response_format_json = build_image_analysis_response_format();

    const std::string content =
        llm.complete_prompt_with_image(build_vision_prompt(), Utils::path_to_utf8(image_path), options);
    const VisionReply reply = parse_vision_reply(content);

    ImageAnalysisResult result;
    result.description = reply.description;
    result.suggested_name = LlavaImageAnalyzer::make_suggested_filename(reply.suggested_name, image_path);
    if (result.suggested_name.empty()) {
        throw std::runtime_error("Visual reply did not contain a usable filename");
    }
    return result;
}

} // namespace ApiImageAnalyzer
