#ifndef HEADLESS_ANALYSIS_COMMAND_HPP
#define HEADLESS_ANALYSIS_COMMAND_HPP

#include "AnalysisRuntimeLock.hpp"

#include <filesystem>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

/**
 * @brief Parses and runs the headless analysis command used by external integrations.
 *
 * The command is intentionally UI-neutral so Windows Explorer and future headless
 * callers can share the same lock and status contract without loading MainApp.
 */
class HeadlessAnalysisCommand {
public:
    /**
     * @brief Operation requested by the external integration.
     */
    enum class Operation {
        Unknown,
        Categorize,
        Rename,
        CategorizeAndRename
    };

    /**
     * @brief Review/apply policy requested by the caller.
     */
    enum class ApplyMode {
        UseSettings,
        ReviewOnly,
        AutoApply
    };

    /**
     * @brief Process exit codes returned by the headless command.
     */
    enum ExitCode {
        Success = 0,
        Failure = 1,
        Usage = 2,
        Busy = 3,
        Unsupported = 4
    };

    /**
     * @brief Parsed headless command options.
     */
    struct Options {
        Operation operation{Operation::Unknown};
        ApplyMode apply_mode{ApplyMode::UseSettings};
        std::vector<std::filesystem::path> paths;
        std::optional<std::filesystem::path> status_file;
        std::string job_id;
    };

    /**
     * @brief Result of command-line parsing.
     */
    struct ParseResult {
        bool requested{false};
        bool help_requested{false};
        Options options;
        std::string error;
        std::vector<bool> consumed_arguments;
    };

    /**
     * @brief Parse headless-related command-line arguments.
     * @param argc Argument count.
     * @param argv Argument values.
     * @return Parsed headless state and consumed argument indices.
     */
    static ParseResult parse(int argc, char** argv);

    /**
     * @brief Returns user-facing command syntax for the headless mode.
     * @return Usage text.
     */
    static std::string usage_text();

    /**
     * @brief Convert an operation to its CLI/status string.
     * @param operation Operation enum value.
     * @return Stable operation string.
     */
    static std::string operation_to_string(Operation operation);

    /**
     * @brief Parse an operation string.
     * @param value Operation string.
     * @return Parsed operation, or Unknown for unsupported values.
     */
    static Operation operation_from_string(const std::string& value);

    /**
     * @brief Run a parsed headless command.
     * @param options Parsed command options.
     * @param runtime_dir Directory used for shared runtime lock state.
     * @param out Stream for machine-readable status output.
     * @param err Stream for diagnostics.
     * @return Process exit code.
     */
    static int run(const Options& options,
                   const std::filesystem::path& runtime_dir,
                   std::ostream& out,
                   std::ostream& err);
};

#endif // HEADLESS_ANALYSIS_COMMAND_HPP
