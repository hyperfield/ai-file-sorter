# Self-documenting code progress

## Documentation standard

- `app/include/*.hpp`: Doxygen comments are required for public and protected classes, structs, enums, and API function declarations.
- `app/lib/*.cpp`: Doxygen comments are optional and should be used selectively for non-trivial internal helpers, parsing logic, algorithms, or code with important invariants. Trivial local helpers do not require full Doxygen blocks.
- Prefer documenting declarations in headers instead of duplicating the same comments on definitions in source files.

## Inventory (scan 2026-01-16)
Heuristic scan for files that may still need documentation work.
This is not a per-symbol audit.

### Headers missing required API docs

- app/include/CategorizationDialog.hpp
- app/include/CategorizationProgressDialog.hpp
- app/include/CategorizationService.hpp
- app/include/CategorizationServiceTestAccess.hpp
- app/include/CategorizationSession.hpp
- app/include/CategoryLanguage.hpp
- app/include/ConsistencyPassService.hpp
- app/include/DatabaseManager.hpp
- app/include/DialogUtils.hpp
- app/include/DryRunPreviewDialog.hpp
- app/include/EmbeddedEnv.hpp
- app/include/ErrorMessages.hpp
- app/include/FileScanner.hpp
- app/include/GeminiClient.hpp
- app/include/ILLMClient.hpp
- app/include/IniConfig.hpp
- app/include/LLMClient.hpp
- app/include/LLMDownloader.hpp
- app/include/LLMErrors.hpp
- app/include/LLMSelectionDialog.hpp
- app/include/LLMSelectionDialogTestAccess.hpp
- app/include/Language.hpp
- app/include/LlamaModelParams.hpp
- app/include/LlavaImageAnalyzer.hpp
- app/include/LocalLLMClient.hpp
- app/include/LocalLLMTestAccess.hpp
- app/include/Logger.hpp
- app/include/MainApp.hpp
- app/include/MainAppEditActions.hpp
- app/include/MainAppHelpActions.hpp
- app/include/MainAppTestAccess.hpp
- app/include/MainAppUiBuilder.hpp
- app/include/MovableCategorizedFile.hpp
- app/include/ResultsCoordinator.hpp
- app/include/Settings.hpp
- app/include/TestHooks.hpp
- app/include/TranslationManager.hpp
- app/include/Types.hpp
- app/include/UiTranslator.hpp
- app/include/UndoManager.hpp
- app/include/Updater.hpp
- app/include/Utils.hpp
- app/include/Version.hpp
- app/include/WhitelistManagerDialog.hpp
- app/include/WhitelistStore.hpp

### Source files with complex internal logic that may need explanatory comments

- app/lib/CategorizationDialog.cpp
- app/lib/CategorizationProgressDialog.cpp
- app/lib/CategorizationService.cpp
- app/lib/CategorizationSession.cpp
- app/lib/ConsistencyPassService.cpp
- app/lib/CustomLLMDialog.cpp
- app/lib/DatabaseManager.cpp
- app/lib/DialogUtils.cpp
- app/lib/DryRunPreviewDialog.cpp
- app/lib/EmbeddedEnv.cpp
- app/lib/FileScanner.cpp
- app/lib/GeminiClient.cpp
- app/lib/IniConfig.cpp
- app/lib/LLMClient.cpp
- app/lib/LLMDownloader.cpp
- app/lib/LLMSelectionDialog.cpp
- app/lib/LlavaImageAnalyzer.cpp
- app/lib/LocalLLMClient.cpp
- app/lib/Logger.cpp
- app/lib/MainApp.cpp
- app/lib/MainAppEditActions.cpp
- app/lib/MainAppHelpActions.cpp
- app/lib/MainAppUiBuilder.cpp
- app/lib/MovableCategorizedFile.cpp
- app/lib/ResultsCoordinator.cpp
- app/lib/Settings.cpp
- app/lib/TranslationManager.cpp
- app/lib/UiTranslator.cpp
- app/lib/UndoManager.cpp
- app/lib/Updater.cpp
- app/lib/Utils.cpp
- app/lib/Version.cpp
- app/lib/WhitelistManagerDialog.cpp
- app/lib/WhitelistStore.cpp
- app/main.cpp
- app/startapp_linux.cpp
- app/startapp_windows.cpp

## Updates
- 2026-01-16: Documented `app/include/CustomLLMDialog.hpp`.
