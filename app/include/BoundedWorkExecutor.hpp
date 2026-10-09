/**
 * @file BoundedWorkExecutor.hpp
 * @brief Runs independent jobs with a hard cap on simultaneous execution.
 *
 * Ownership model:
 *  - The caller owns `contexts` (for example one LLM client per slot). Worker `w` is the only
 *    thread that touches `contexts[w]` during a batch, so contexts never need locking.
 *  - Each job writes only its own `Outcome` slot, indexed by input position. Results therefore
 *    come back in input order regardless of completion order.
 *  - run_batch() joins every worker before returning, so the calling thread may safely use any
 *    context and commit results afterwards. Work functions must not touch Qt widgets, the
 *    database, or other shared mutable state; the caller commits results itself.
 */
#pragma once

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <exception>
#include <functional>
#include <optional>
#include <system_error>
#include <thread>
#include <type_traits>
#include <vector>

namespace BoundedWorkExecutor {

/**
 * @brief Outcome of one job in a batch.
 *
 * `attempted()` is false only when stop was requested before the job began, in which case
 * the job has no effects and must not be committed.
 */
template <typename Result>
struct Outcome {
    std::optional<Result> value;
    std::exception_ptr error;

    bool attempted() const { return value.has_value() || static_cast<bool>(error); }
};

/**
 * @brief Runs `work(contexts[w], index, inputs[index])` for every input.
 *
 * At most `contexts.size()` jobs are active at any moment. Jobs are claimed in input order.
 * When `stop` becomes true, no further jobs begin; jobs already running finish normally.
 *
 * @param inputs Job inputs; referenced, not copied.
 * @param contexts Per-worker state; the batch uses min(contexts.size(), inputs.size()) workers.
 * @param stop Cancellation flag checked before each job is claimed.
 * @param work Callable with signature `Result(Context&, size_t, const Input&)`.
 * @return One outcome per input, in input order.
 */
template <typename Input, typename Context, typename Work>
auto run_batch(const std::vector<Input>& inputs,
               std::vector<Context>& contexts,
               const std::atomic<bool>& stop,
               Work&& work)
    -> std::vector<Outcome<std::invoke_result_t<Work&, Context&, size_t, const Input&>>>
{
    using Result = std::invoke_result_t<Work&, Context&, size_t, const Input&>;

    std::vector<Outcome<Result>> outcomes(inputs.size());
    if (inputs.empty() || contexts.empty()) {
        return outcomes;
    }

    std::atomic<size_t> next_index{0};
    auto run_worker = [&](size_t worker_index) {
        for (;;) {
            if (stop.load()) {
                return;
            }
            const size_t index = next_index.fetch_add(1);
            if (index >= inputs.size()) {
                return;
            }
            try {
                outcomes[index].value.emplace(work(contexts[worker_index], index, inputs[index]));
            } catch (...) {
                outcomes[index].error = std::current_exception();
            }
        }
    };

    const size_t worker_count = std::min(contexts.size(), inputs.size());
    std::vector<std::thread> threads;
    threads.reserve(worker_count - 1);
    for (size_t worker = 1; worker < worker_count; ++worker) {
        try {
            threads.emplace_back(run_worker, worker);
        } catch (const std::system_error&) {
            // Could not start another thread; the workers already running will drain the queue.
            break;
        }
    }
    // The calling thread serves as worker 0 so a batch of one needs no extra thread.
    run_worker(0);
    for (auto& thread : threads) {
        thread.join();
    }
    return outcomes;
}

} // namespace BoundedWorkExecutor
