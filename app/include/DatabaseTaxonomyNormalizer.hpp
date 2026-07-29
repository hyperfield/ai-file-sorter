#ifndef DATABASE_TAXONOMY_NORMALIZER_HPP
#define DATABASE_TAXONOMY_NORMALIZER_HPP

#include <string>

namespace DatabaseTaxonomyNormalizer {

/**
 * @brief Canonicalized category labels used for taxonomy lookup and storage.
 */
struct NormalizedCategory {
    std::string category;
    std::string subcategory;
    std::string normalized_category;
    std::string normalized_subcategory;
    std::string match_subcategory;
};

/**
 * @brief Normalize a category label for taxonomy comparisons.
 * @param input Display label to normalize.
 * @return Lowercase alphanumeric label with normalized spacing.
 */
std::string normalize_label(const std::string& input);

/**
 * @brief Strip generic trailing file-type stopwords from an already normalized label.
 * @param normalized Normalized category or subcategory label.
 * @return Label without trailing generic stopwords when doing so preserves content.
 */
std::string strip_trailing_stopwords(const std::string& normalized);

/**
 * @brief Canonicalize category and subcategory labels for taxonomy resolution.
 * @param category User-facing main category label.
 * @param subcategory User-facing subcategory label.
 * @return Display and normalized labels, plus the subcategory form used for matching.
 */
NormalizedCategory normalize_category(std::string category, std::string subcategory);

/**
 * @brief Calculate normalized edit-distance similarity for taxonomy fuzzy matching.
 * @param a First normalized label.
 * @param b Second normalized label.
 * @return Similarity score in the range 0.0 to 1.0.
 */
double string_similarity(const std::string& a, const std::string& b);

} // namespace DatabaseTaxonomyNormalizer

#endif // DATABASE_TAXONOMY_NORMALIZER_HPP
