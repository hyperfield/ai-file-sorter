#include "VisualModelCatalog.hpp"

#include <algorithm>

const std::vector<VisualModelDescriptor>& visual_model_descriptors()
{
    static const std::vector<VisualModelDescriptor> descriptors = {
        {
            "llava-v1.6-mistral-7b",
            "LLaVA 1.6 Mistral 7B",
            VisualModelArchitecture::MtmdProjector,
            VisualPromptPolicy::LegacyLlava,
            {
                {VisualModelArtifactKind::Model,
                 "LLaVA 1.6 Mistral 7B (text model)",
                 "LLAVA_MODEL_URL",
                 {}},
                {VisualModelArtifactKind::Mmproj,
                 "LLaVA mmproj (vision encoder)",
                 "LLAVA_MMPROJ_URL",
                 {"mmproj-model-f16.gguf",
                  "llava-v1.6-mistral-7b-mmproj-f16.gguf"}},
            },
        },
        {
            "llava-v1.6-vicuna-7b",
            "LLaVA 1.6 Vicuna 7B",
            VisualModelArchitecture::MtmdProjector,
            VisualPromptPolicy::LegacyLlava,
            {
                {VisualModelArtifactKind::Model,
                 "LLaVA 1.6 Vicuna 7B (text model)",
                 "LLAVA_VICUNA_MODEL_URL",
                 {}},
                {VisualModelArtifactKind::Mmproj,
                 "LLaVA Vicuna mmproj (vision encoder)",
                 "LLAVA_VICUNA_MMPROJ_URL",
                 {"mmproj-model-f16.gguf",
                 "llava-v1.6-vicuna-7b-mmproj-model-f16.gguf",
                  "llava-v1.6-vicuna-7b-mmproj-f16.gguf"}},
            },
        },
        {
            "gemma-3-4b-it",
            "Gemma 3 4B IT",
            VisualModelArchitecture::MtmdProjector,
            VisualPromptPolicy::StructuredVisionInstruct,
            {
                {VisualModelArtifactKind::Model,
                 "Gemma 3 4B IT (text model)",
                 "GEMMA3_4B_MODEL_URL",
                 {}},
                {VisualModelArtifactKind::Mmproj,
                 "Gemma 3 4B mmproj (vision encoder)",
                 "GEMMA3_4B_MMPROJ_URL",
                 {"mmproj-model.gguf",
                  "mmproj-model-f16.gguf",
                  "mmproj-gemma-3-4b-it-Q4_0.gguf",
                  "mmproj-gemma-3-4b-it-Q4_K_M.gguf",
                  "mmproj-gemma-3-4b-it-Q8_0.gguf"}},
            },
        },
    };
    return descriptors;
}

const VisualModelDescriptor* find_visual_model_descriptor(std::string_view id)
{
    const auto& descriptors = visual_model_descriptors();
    const auto it = std::find_if(descriptors.begin(), descriptors.end(),
                                 [id](const VisualModelDescriptor& descriptor) {
                                     return descriptor.id == id;
                                 });
    if (it == descriptors.end()) {
        return nullptr;
    }
    return &(*it);
}

const VisualModelDescriptor& default_visual_model_descriptor()
{
    return visual_model_descriptors().front();
}
