#include "ImageAnalyzerFactory.hpp"

#include "LlavaImageAnalyzer.hpp"

#include <stdexcept>

namespace {

std::filesystem::path require_artifact_path(const VisualLlmRuntime::Backend& backend,
                                            VisualModelArtifactKind kind,
                                            const char* label)
{
    const auto path = backend.path_for(kind);
    if (!path.has_value()) {
        throw std::runtime_error(std::string("Visual backend is missing required artifact: ") + label);
    }
    return *path;
}

} // namespace

std::unique_ptr<ImageAnalyzer> ImageAnalyzerFactory::create(const VisualLlmRuntime::Backend& backend,
                                                            ImageAnalyzerSettings settings)
{
    if (!backend.descriptor) {
        throw std::runtime_error("Visual backend descriptor is missing.");
    }

    switch (backend.descriptor->architecture) {
    case VisualModelArchitecture::MtmdProjector: {
        const auto model_path =
            require_artifact_path(backend, VisualModelArtifactKind::Model, "model");
        const auto mmproj_path =
            require_artifact_path(backend, VisualModelArtifactKind::Mmproj, "mmproj");
        return std::make_unique<LlavaImageAnalyzer>(
            model_path,
            mmproj_path,
            backend.descriptor->prompt_policy,
            std::move(settings));
    }
    }

    throw std::runtime_error("Unsupported visual model architecture.");
}
