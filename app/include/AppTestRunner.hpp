#pragma once

#include <string>
#include <vector>

/**
 * @brief Runs built-in app self-tests from the production executable.
 *
 * These tests exercise production services with deterministic stubs and
 * temporary app data. They are intended for developer/runtime validation
 * outside the Catch2 unit-test binary.
 */
class AppTestRunner {
public:
    /**
     * @brief Options for a self-test run.
     */
    struct Options {
        /** @brief Suite selector: `all`, `whitelist`, or `tls` (Linux/macOS). */
        std::string suite{"all"};
    };

    /**
     * @brief Result for one self-test case.
     */
    struct CaseResult {
        /** @brief Human-readable test case name. */
        std::string name;
        /** @brief True when all expectations passed. */
        bool passed{false};
        /** @brief Diagnostic detail for pass/fail output. */
        std::string message;
    };

    /**
     * @brief Aggregate result for a self-test run.
     */
    struct Result {
        /** @brief Selected suite name as normalized by the runner. */
        std::string suite;
        /** @brief Individual test case results. */
        std::vector<CaseResult> cases;
        /** @brief General error when the suite cannot be executed. */
        std::string error;

        /**
         * @brief Returns whether the run completed with no failures.
         * @return True when there is no general error and every case passed.
         */
        bool passed() const;
    };

    /**
     * @brief Run the requested self-test suite.
     * @param options Suite selection and future runner options.
     * @return Aggregate self-test result.
     */
    Result run(const Options& options) const;

    /**
     * @brief Return supported self-test suite names.
     * @return Suite identifiers accepted by @ref run.
     */
    static std::vector<std::string> available_suites();
};
