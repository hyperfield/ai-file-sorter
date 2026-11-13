#include "TextEncoding.hpp"
#include "TextEmbeddingService.hpp"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <iomanip>
#include <numeric>
#include <sstream>
#include <string>

namespace {

constexpr std::string_view kModelId = "local-hash-v1";
constexpr std::size_t kDimension = 128;
constexpr std::uint64_t kFnvOffsetBasis = 14695981039346656037ull;
constexpr std::uint64_t kFnvPrime = 1099511628211ull;

std::uint64_t fnv1a(std::string_view value)
{
    std::uint64_t hash = kFnvOffsetBasis;
    for (unsigned char ch : value) {
        hash ^= static_cast<std::uint64_t>(ch);
        hash *= kFnvPrime;
    }
    return hash;
}

std::string normalize_token(std::string token)
{
    for (auto& ch : token) {
        const auto byte = static_cast<unsigned char>(ch);
        if (byte < 128) {
            ch = static_cast<char>(TextEncoding::ascii_lower(byte));
        }
    }

    if (token.size() > 4 && token.ends_with("ies")) {
        token.erase(token.size() - 3);
        token.push_back('y');
    } else if (token.size() > 4 && token.ends_with("es")) {
        token.erase(token.size() - 2);
    } else if (token.size() > 3 && token.back() == 's') {
        token.pop_back();
    }
    return token;
}

std::vector<std::string> tokenize(std::string_view text)
{
    std::vector<std::string> tokens;
    std::string current;
    for (unsigned char ch : text) {
        if (TextEncoding::ascii_alnum(ch) || ch >= 128) {
            current.push_back(static_cast<char>(ch));
            continue;
        }
        if (!current.empty()) {
            tokens.push_back(normalize_token(std::move(current)));
            current.clear();
        }
    }
    if (!current.empty()) {
        tokens.push_back(normalize_token(std::move(current)));
    }
    tokens.erase(std::remove_if(tokens.begin(),
                                tokens.end(),
                                [](const std::string& token) {
                                    return token.size() < 2;
                                }),
                 tokens.end());
    return tokens;
}

void normalize_vector(std::vector<float>& vector)
{
    const double squared_sum = std::accumulate(vector.begin(), vector.end(), 0.0, [](double sum, float value) {
        return sum + static_cast<double>(value) * static_cast<double>(value);
    });
    if (squared_sum <= 0.0) {
        return;
    }

    const auto norm = static_cast<float>(std::sqrt(squared_sum));
    for (auto& value : vector) {
        value /= norm;
    }
}

} // namespace

std::string_view TextEmbeddingService::model_id()
{
    return kModelId;
}

std::size_t TextEmbeddingService::dimension()
{
    return kDimension;
}

std::vector<float> TextEmbeddingService::embed(std::string_view text)
{
    std::vector<float> vector(kDimension, 0.0f);
    for (const auto& token : tokenize(text)) {
        const std::uint64_t hash = fnv1a(token);
        const auto index = static_cast<std::size_t>(hash % kDimension);
        const float sign = ((hash >> 63u) & 1u) == 0u ? 1.0f : -1.0f;
        vector[index] += sign;
    }
    normalize_vector(vector);
    return vector;
}

double TextEmbeddingService::cosine_similarity(const std::vector<float>& lhs,
                                               const std::vector<float>& rhs)
{
    if (lhs.size() != rhs.size() || lhs.empty()) {
        return 0.0;
    }

    double dot = 0.0;
    double lhs_squared = 0.0;
    double rhs_squared = 0.0;
    for (std::size_t i = 0; i < lhs.size(); ++i) {
        dot += static_cast<double>(lhs[i]) * static_cast<double>(rhs[i]);
        lhs_squared += static_cast<double>(lhs[i]) * static_cast<double>(lhs[i]);
        rhs_squared += static_cast<double>(rhs[i]) * static_cast<double>(rhs[i]);
    }
    if (lhs_squared <= 0.0 || rhs_squared <= 0.0) {
        return 0.0;
    }
    return dot / (std::sqrt(lhs_squared) * std::sqrt(rhs_squared));
}

std::string TextEmbeddingService::source_hash(std::string_view text)
{
    std::ostringstream oss;
    oss << std::hex << std::setfill('0') << std::setw(16) << fnv1a(text);
    return oss.str();
}
