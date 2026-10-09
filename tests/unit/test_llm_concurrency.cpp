#include <catch2/catch_test_macros.hpp>

#include "BoundedWorkExecutor.hpp"
#include "CategoryLanguage.hpp"
#include "CategorizationService.hpp"
#include "DatabaseManager.hpp"
#include "ILLMClient.hpp"
#include "Settings.hpp"
#include "TestHelpers.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

// Shared observation point for every client created by one categorization run.
struct LlmProbe {
    std::function<std::chrono::milliseconds(const std::string&)> delay_for =
        [](const std::string&) { return std::chrono::milliseconds(60); };
    std::function<void(const std::string&)> on_call_start;
    std::string fail_for;

    std::atomic<int> active{0};
    std::atomic<int> max_active{0};
    std::atomic<int> calls{0};
    std::atomic<int> instances{0};
    /// Incremented when one client object is entered by two threads at once.
    std::atomic<int> shared_client_violations{0};
};

class ProbeLLM : public ILLMClient {
public:
    explicit ProbeLLM(std::shared_ptr<LlmProbe> probe) : probe_(std::move(probe)) {
        ++probe_->instances;
    }

    std::string categorize_file(const std::string& file_name,
                                const std::string&,
                                FileType,
                                const std::string&) override {
        if (in_use_.exchange(true)) {
            ++probe_->shared_client_violations;
        }
        struct Scope {
            LlmProbe& probe;
            std::atomic<bool>& in_use;
            ~Scope() {
                in_use.store(false);
                --probe.active;
            }
        } scope{*probe_, in_use_};

        const int now_active = ++probe_->active;
        int observed = probe_->max_active.load();
        while (now_active > observed && !probe_->max_active.compare_exchange_weak(observed, now_active)) {
        }
        ++probe_->calls;
        if (probe_->on_call_start) {
            probe_->on_call_start(file_name);
        }
        std::this_thread::sleep_for(probe_->delay_for(file_name));
        if (!probe_->fail_for.empty() && file_name == probe_->fail_for) {
            throw std::runtime_error("scripted failure for " + file_name);
        }
        return "Documents : Reports";
    }

    std::string complete_prompt(const std::string&, int) override {
        return "Documents : Reports";
    }

    void set_prompt_logging_enabled(bool) override {}

private:
    std::shared_ptr<LlmProbe> probe_;
    std::atomic<bool> in_use_{false};
};

std::function<std::unique_ptr<ILLMClient>()> make_probe_factory(const std::shared_ptr<LlmProbe>& probe) {
    return [probe]() -> std::unique_ptr<ILLMClient> {
        return std::make_unique<ProbeLLM>(probe);
    };
}

std::vector<FileEntry> make_entries(const std::filesystem::path& dir, const std::vector<std::string>& names) {
    std::vector<FileEntry> entries;
    entries.reserve(names.size());
    for (const auto& name : names) {
        entries.push_back(FileEntry{(dir / name).string(), name, FileType::File});
    }
    return entries;
}

void configure_remote_settings(Settings& settings, int concurrency) {
    settings.set_llm_choice(LLMChoice::Remote_OpenAI);
    settings.set_openai_api_key("test-key");
    settings.set_category_language(CategoryLanguage::English);
    settings.set_llm_concurrency(concurrency);
}

// Records which threads entered each client. Clients are created on the coordinator thread in
// slot order, so instance 0 is the slot that worker 0 (the coordinator thread) always serves.
struct ThreadTrace {
    std::mutex mutex;
    std::vector<std::set<std::thread::id>> ids_by_instance;
    std::set<std::thread::id> all_ids;
    std::atomic<int> calls{0};
};

class ThreadRecordingLLM : public ILLMClient {
public:
    explicit ThreadRecordingLLM(std::shared_ptr<ThreadTrace> trace) : trace_(std::move(trace)) {
        std::lock_guard<std::mutex> lock(trace_->mutex);
        index_ = trace_->ids_by_instance.size();
        trace_->ids_by_instance.emplace_back();
    }

    std::string categorize_file(const std::string&,
                                const std::string&,
                                FileType,
                                const std::string&) override {
        const auto id = std::this_thread::get_id();
        {
            std::lock_guard<std::mutex> lock(trace_->mutex);
            trace_->ids_by_instance[index_].insert(id);
            trace_->all_ids.insert(id);
        }
        ++trace_->calls;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        return "Documents : Reports";
    }

    std::string complete_prompt(const std::string&, int) override {
        return "Documents : Reports";
    }

    void set_prompt_logging_enabled(bool) override {}

private:
    std::shared_ptr<ThreadTrace> trace_;
    size_t index_ = 0;
};

} // namespace

TEST_CASE("LLM concurrency setting defaults to one and clamps to supported levels") {
    TempDir config_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", config_dir.path().string());

    Settings settings;
    settings.load();
    REQUIRE(settings.get_llm_concurrency() == 1);

    settings.set_llm_concurrency(2);
    REQUIRE(settings.get_llm_concurrency() == 2);
    settings.set_llm_concurrency(4);
    REQUIRE(settings.get_llm_concurrency() == 4);
    settings.set_llm_concurrency(1);
    REQUIRE(settings.get_llm_concurrency() == 1);

    settings.set_llm_concurrency(0);
    REQUIRE(settings.get_llm_concurrency() == 1);
    settings.set_llm_concurrency(-3);
    REQUIRE(settings.get_llm_concurrency() == 1);
    settings.set_llm_concurrency(3);
    REQUIRE(settings.get_llm_concurrency() == 2);
    settings.set_llm_concurrency(6);
    REQUIRE(settings.get_llm_concurrency() == 6);
    settings.set_llm_concurrency(8);
    REQUIRE(settings.get_llm_concurrency() == 8);
    settings.set_llm_concurrency(7);
    REQUIRE(settings.get_llm_concurrency() == 6);
    settings.set_llm_concurrency(16);
    REQUIRE(settings.get_llm_concurrency() == 8);
    settings.set_llm_concurrency(1000);
    REQUIRE(settings.get_llm_concurrency() == 8);
}

TEST_CASE("LLM concurrency persists and reloads with clamping applied") {
    TempDir config_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", config_dir.path().string());

    Settings settings;
    settings.load();
    settings.set_llm_concurrency(4);
    REQUIRE(settings.save());

    Settings reloaded;
    reloaded.load();
    REQUIRE(reloaded.get_llm_concurrency() == 4);

    reloaded.set_llm_concurrency(2);
    REQUIRE(reloaded.save());
    Settings second;
    second.load();
    REQUIRE(second.get_llm_concurrency() == 2);
}

TEST_CASE("Effective LLM concurrency applies only to OpenAI-compatible remote choices") {
    Settings settings;
    settings.set_llm_concurrency(4);

    settings.set_llm_choice(LLMChoice::Remote_OpenAI);
    REQUIRE(settings.effective_llm_concurrency() == 4);
    settings.set_llm_choice(LLMChoice::Remote_Custom);
    REQUIRE(settings.effective_llm_concurrency() == 4);
    settings.set_llm_choice(LLMChoice::Remote_Gemini);
    REQUIRE(settings.effective_llm_concurrency() == 1);
    settings.set_llm_choice(LLMChoice::Local_4b_Gemma);
    REQUIRE(settings.effective_llm_concurrency() == 1);
    settings.set_llm_choice(LLMChoice::Custom);
    REQUIRE(settings.effective_llm_concurrency() == 1);
}

TEST_CASE("BoundedWorkExecutor never runs more jobs than contexts and keeps input order") {
    std::vector<int> inputs(8);
    for (int i = 0; i < 8; ++i) {
        inputs[static_cast<size_t>(i)] = i;
    }
    std::vector<int> contexts(4, 0);
    std::atomic<int> active{0};
    std::atomic<int> max_active{0};

    const std::atomic<bool> stop{false};
    const auto outcomes = BoundedWorkExecutor::run_batch(
        inputs,
        contexts,
        stop,
        [&](int&, size_t, const int& value) {
            const int now = ++active;
            int observed = max_active.load();
            while (now > observed && !max_active.compare_exchange_weak(observed, now)) {
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(40));
            --active;
            return value * 10;
        });

    REQUIRE(outcomes.size() == inputs.size());
    REQUIRE(max_active.load() > 1);
    REQUIRE(max_active.load() <= 4);
    for (size_t i = 0; i < outcomes.size(); ++i) {
        REQUIRE(outcomes[i].attempted());
        REQUIRE(outcomes[i].value.value() == static_cast<int>(i) * 10);
    }
}

TEST_CASE("BoundedWorkExecutor returns input order when jobs finish in reverse") {
    std::vector<int> inputs = {0, 1, 2, 3};
    std::vector<int> contexts(4, 0);
    std::mutex order_mutex;
    std::vector<int> completion_order;
    const std::atomic<bool> stop{false};

    const auto outcomes = BoundedWorkExecutor::run_batch(
        inputs,
        contexts,
        stop,
        [&](int&, size_t index, const int& value) {
            std::this_thread::sleep_for(std::chrono::milliseconds(30 * (4 - static_cast<int>(index))));
            std::lock_guard<std::mutex> lock(order_mutex);
            completion_order.push_back(value);
            return value;
        });

    REQUIRE(completion_order.size() == 4);
    REQUIRE(completion_order.front() == 3);
    for (size_t i = 0; i < outcomes.size(); ++i) {
        REQUIRE(outcomes[i].value.value() == static_cast<int>(i));
    }
}

TEST_CASE("BoundedWorkExecutor isolates a failing job and stop prevents new jobs") {
    std::vector<int> inputs = {0, 1, 2, 3};
    std::vector<int> contexts(2, 0);
    const std::atomic<bool> stop{false};

    const auto outcomes = BoundedWorkExecutor::run_batch(
        inputs,
        contexts,
        stop,
        [](int&, size_t, const int& value) -> int {
            if (value == 1) {
                throw std::runtime_error("job one failed");
            }
            return value;
        });

    REQUIRE(outcomes[0].value.value() == 0);
    REQUIRE(static_cast<bool>(outcomes[1].error));
    REQUIRE(outcomes[2].value.value() == 2);
    REQUIRE(outcomes[3].value.value() == 3);

    const std::atomic<bool> already_stopped{true};
    std::atomic<int> started{0};
    const auto skipped = BoundedWorkExecutor::run_batch(
        inputs,
        contexts,
        already_stopped,
        [&](int&, size_t, const int& value) {
            ++started;
            return value;
        });
    REQUIRE(started.load() == 0);
    for (const auto& outcome : skipped) {
        REQUIRE_FALSE(outcome.attempted());
    }
}

TEST_CASE("Sequential categorization (concurrency 1) uses one client and one request at a time") {
    TempDir config_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", config_dir.path().string());
    Settings settings;
    configure_remote_settings(settings, 1);
    DatabaseManager db(settings.get_config_dir());
    CategorizationService service(settings, db, nullptr);

    auto probe = std::make_shared<LlmProbe>();
    TempDir data_dir;
    const auto files = make_entries(data_dir.path(), {"a.txt", "b.txt", "c.txt", "d.txt", "e.txt", "f.txt"});
    std::atomic<bool> stop{false};

    const auto categorized = service.categorize_entries(files,
                                                        false,
                                                        stop,
                                                        {},
                                                        {},
                                                        {},
                                                        {},
                                                        make_probe_factory(probe));
    REQUIRE(categorized.size() == files.size());
    REQUIRE(probe->instances.load() == 1);
    REQUIRE(probe->max_active.load() == 1);
}

TEST_CASE("Concurrent categorization runs requests in parallel within the configured cap") {
    TempDir config_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", config_dir.path().string());
    Settings settings;
    configure_remote_settings(settings, 4);
    DatabaseManager db(settings.get_config_dir());
    CategorizationService service(settings, db, nullptr);

    auto probe = std::make_shared<LlmProbe>();
    probe->delay_for = [](const std::string&) { return std::chrono::milliseconds(120); };
    TempDir data_dir;
    const auto files = make_entries(data_dir.path(),
                                    {"f1.txt", "f2.txt", "f3.txt", "f4.txt", "f5.txt", "f6.txt", "f7.txt", "f8.txt"});
    std::atomic<bool> stop{false};

    const auto categorized = service.categorize_entries(files,
                                                        false,
                                                        stop,
                                                        {},
                                                        {},
                                                        {},
                                                        {},
                                                        make_probe_factory(probe));
    REQUIRE(categorized.size() == files.size());
    REQUIRE(probe->max_active.load() > 1);
    REQUIRE(probe->max_active.load() <= 4);
    REQUIRE(probe->shared_client_violations.load() == 0);
}

TEST_CASE("Concurrent categorization with concurrency 2 never exceeds two requests") {
    TempDir config_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", config_dir.path().string());
    Settings settings;
    configure_remote_settings(settings, 2);
    DatabaseManager db(settings.get_config_dir());
    CategorizationService service(settings, db, nullptr);

    auto probe = std::make_shared<LlmProbe>();
    probe->delay_for = [](const std::string&) { return std::chrono::milliseconds(80); };
    TempDir data_dir;
    const auto files = make_entries(data_dir.path(), {"g1.txt", "g2.txt", "g3.txt", "g4.txt", "g5.txt", "g6.txt"});
    std::atomic<bool> stop{false};

    const auto categorized = service.categorize_entries(files,
                                                        false,
                                                        stop,
                                                        {},
                                                        {},
                                                        {},
                                                        {},
                                                        make_probe_factory(probe));
    REQUIRE(categorized.size() == files.size());
    REQUIRE(probe->max_active.load() <= 2);
    REQUIRE(probe->instances.load() == 2);
}

TEST_CASE("Concurrent categorization creates one client per slot, not per file") {
    TempDir config_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", config_dir.path().string());
    Settings settings;
    configure_remote_settings(settings, 4);
    DatabaseManager db(settings.get_config_dir());
    CategorizationService service(settings, db, nullptr);

    auto probe = std::make_shared<LlmProbe>();
    TempDir data_dir;
    std::vector<std::string> names;
    for (int i = 0; i < 20; ++i) {
        names.push_back("h" + std::to_string(i) + ".txt");
    }
    const auto files = make_entries(data_dir.path(), names);
    std::atomic<bool> stop{false};

    const auto categorized = service.categorize_entries(files,
                                                        false,
                                                        stop,
                                                        {},
                                                        {},
                                                        {},
                                                        {},
                                                        make_probe_factory(probe));
    REQUIRE(categorized.size() == files.size());
    REQUIRE(probe->instances.load() <= 4);
    REQUIRE(probe->instances.load() >= 1);
    REQUIRE(probe->shared_client_violations.load() == 0);
}

TEST_CASE("Concurrent categorization commits results and completions in input order") {
    TempDir config_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", config_dir.path().string());
    Settings settings;
    configure_remote_settings(settings, 4);
    DatabaseManager db(settings.get_config_dir());
    CategorizationService service(settings, db, nullptr);

    auto probe = std::make_shared<LlmProbe>();
    // Finish order is the reverse of input order.
    probe->delay_for = [](const std::string& name) {
        if (name == "order_a.txt") return std::chrono::milliseconds(300);
        if (name == "order_b.txt") return std::chrono::milliseconds(200);
        if (name == "order_c.txt") return std::chrono::milliseconds(100);
        return std::chrono::milliseconds(10);
    };
    TempDir data_dir;
    const auto files = make_entries(data_dir.path(),
                                    {"order_a.txt", "order_b.txt", "order_c.txt", "order_d.txt"});
    std::atomic<bool> stop{false};
    std::vector<std::string> completions;
    std::vector<std::string> results;

    const auto categorized = service.categorize_entries(
        files,
        false,
        stop,
        {},
        {},
        [&completions](const FileEntry& entry) { completions.push_back(entry.file_name); },
        {},
        make_probe_factory(probe),
        {},
        {},
        [&results](const CategorizedFile& entry) { results.push_back(entry.file_name); });

    REQUIRE(categorized.size() == 4);
    REQUIRE(categorized[0].file_name == "order_a.txt");
    REQUIRE(categorized[1].file_name == "order_b.txt");
    REQUIRE(categorized[2].file_name == "order_c.txt");
    REQUIRE(categorized[3].file_name == "order_d.txt");
    REQUIRE(results == std::vector<std::string>{"order_a.txt", "order_b.txt", "order_c.txt", "order_d.txt"});
    REQUIRE(completions == std::vector<std::string>{"order_a.txt", "order_b.txt", "order_c.txt", "order_d.txt"});
}

TEST_CASE("Concurrent categorization failure reports once, does not duplicate completions, and propagates") {
    TempDir config_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", config_dir.path().string());
    Settings settings;
    configure_remote_settings(settings, 4);
    DatabaseManager db(settings.get_config_dir());
    CategorizationService service(settings, db, nullptr);

    auto probe = std::make_shared<LlmProbe>();
    probe->fail_for = "fail_b.txt";
    probe->delay_for = [](const std::string&) { return std::chrono::milliseconds(80); };
    TempDir data_dir;
    const auto files = make_entries(data_dir.path(), {"fail_a.txt", "fail_b.txt", "fail_c.txt", "fail_d.txt"});
    std::atomic<bool> stop{false};
    std::vector<std::string> completions;
    std::vector<std::string> progress;

    REQUIRE_THROWS_AS(service.categorize_entries(
                          files,
                          false,
                          stop,
                          [&progress](const std::string& message) { progress.push_back(message); },
                          {},
                          [&completions](const FileEntry& entry) { completions.push_back(entry.file_name); },
                          {},
                          make_probe_factory(probe)),
                      std::runtime_error);

    // Requests for the whole chunk were started, including the two after the failing one.
    REQUIRE(probe->calls.load() == 4);
    // Only entries committed before the failure complete, each at most once.
    REQUIRE(completions == std::vector<std::string>{"fail_a.txt"});
    const auto error_lines = std::count_if(progress.begin(), progress.end(), [](const std::string& message) {
        return message.find("[LLM-ERROR]") != std::string::npos;
    });
    REQUIRE(error_lines == 1);
}

TEST_CASE("Concurrent categorization honours stop: no new requests, no deadlock, unique completions") {
    TempDir config_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", config_dir.path().string());
    Settings settings;
    configure_remote_settings(settings, 4);
    DatabaseManager db(settings.get_config_dir());
    CategorizationService service(settings, db, nullptr);

    std::atomic<bool> stop{false};
    auto probe = std::make_shared<LlmProbe>();
    probe->delay_for = [](const std::string&) { return std::chrono::milliseconds(150); };
    probe->on_call_start = [&stop, probe](const std::string&) {
        if (probe->calls.load() >= 4) {
            stop.store(true);
        }
    };

    TempDir data_dir;
    std::vector<std::string> names;
    for (int i = 0; i < 24; ++i) {
        names.push_back("s" + std::to_string(i) + ".txt");
    }
    const auto files = make_entries(data_dir.path(), names);
    std::vector<std::string> queued;
    std::vector<std::string> completions;

    const auto categorized = service.categorize_entries(
        files,
        false,
        stop,
        {},
        [&queued](const FileEntry& entry) { queued.push_back(entry.file_name); },
        [&completions](const FileEntry& entry) { completions.push_back(entry.file_name); },
        {},
        make_probe_factory(probe));

    REQUIRE(probe->calls.load() < static_cast<int>(files.size()));
    REQUIRE(probe->calls.load() >= 4);
    const std::set<std::string> unique_completions(completions.begin(), completions.end());
    REQUIRE(unique_completions.size() == completions.size());
    REQUIRE(categorized.size() <= completions.size());
    // Every queued item, including those skipped after Stop, must receive exactly one completion.
    REQUIRE(completions.size() == queued.size());
    const std::set<std::string> unique_queued(queued.begin(), queued.end());
    REQUIRE(unique_completions == unique_queued);
}

TEST_CASE("Concurrent categorization calls the client on the worker thread without spawning a helper thread") {
    TempDir config_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", config_dir.path().string());
    Settings settings;
    configure_remote_settings(settings, 2);
    DatabaseManager db(settings.get_config_dir());
    CategorizationService service(settings, db, nullptr);

    auto trace = std::make_shared<ThreadTrace>();
    std::function<std::unique_ptr<ILLMClient>()> factory = [trace]() -> std::unique_ptr<ILLMClient> {
        return std::make_unique<ThreadRecordingLLM>(trace);
    };

    TempDir data_dir;
    const auto files = make_entries(data_dir.path(), {"t1.txt", "t2.txt", "t3.txt", "t4.txt", "t5.txt", "t6.txt"});
    std::atomic<bool> stop{false};

    const auto categorized = service.categorize_entries(files,
                                                        false,
                                                        stop,
                                                        {},
                                                        {},
                                                        {},
                                                        {},
                                                        factory);
    REQUIRE(categorized.size() == files.size());
    REQUIRE(trace->calls.load() == 6);
    REQUIRE(trace->ids_by_instance.size() == 2);
    // The coordinator thread is worker 0 and always serves clients[0], so that client never sees another thread.
    REQUIRE(trace->ids_by_instance[0].size() == 1);
    // Each chunk of two files may start one new worker thread for its batch and joins it before returning.
    // That bounds the distinct ids to the coordinator plus one helper per chunk (3 chunks here). A detached
    // thread per request would give six distinct ids.
    const size_t chunks = (files.size() + 1) / 2;
    REQUIRE(trace->all_ids.size() <= 1 + chunks);
}
