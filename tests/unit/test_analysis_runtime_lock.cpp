#include <catch2/catch_test_macros.hpp>

#include "AnalysisRuntimeLock.hpp"

#include <QTemporaryDir>

TEST_CASE("AnalysisRuntimeLock serializes active jobs and persists metadata")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());

    AnalysisRuntimeLock lock(dir.path().toStdString());
    AnalysisRuntimeLock::Metadata metadata;
    metadata.owner = AnalysisRuntimeLock::Owner::Gui;
    metadata.job_id = "job-1";
    metadata.description = "GUI analysis";

    std::string error;
    auto lease = lock.try_acquire(metadata, &error);
    REQUIRE(lease.has_value());
    CHECK(error.empty());
    CHECK(lease->owns_lock());
    CHECK(lease->metadata().owner == AnalysisRuntimeLock::Owner::Gui);
    CHECK(lease->metadata().pid > 0);
    CHECK_FALSE(lease->metadata().started_at_utc.empty());

    AnalysisRuntimeLock::Metadata stored;
    CHECK(lock.is_locked(&stored));
    CHECK(stored.owner == AnalysisRuntimeLock::Owner::Gui);
    CHECK(stored.job_id == "job-1");
    CHECK(stored.description == "GUI analysis");

    AnalysisRuntimeLock competing_lock(dir.path().toStdString());
    auto competing = competing_lock.try_acquire(
        AnalysisRuntimeLock::Metadata{AnalysisRuntimeLock::Owner::ExplorerWorker,
                                      0,
                                      "job-2",
                                      {},
                                      "Explorer analysis"},
        &error);
    CHECK_FALSE(competing.has_value());
    CHECK_FALSE(error.empty());

    lease->release();
    CHECK_FALSE(lock.is_locked());

    competing = competing_lock.try_acquire(
        AnalysisRuntimeLock::Metadata{AnalysisRuntimeLock::Owner::ExplorerWorker,
                                      0,
                                      "job-2",
                                      {},
                                      "Explorer analysis"},
        &error);
    REQUIRE(competing.has_value());
    CHECK(competing->metadata().owner == AnalysisRuntimeLock::Owner::ExplorerWorker);
}

TEST_CASE("AnalysisRuntimeLock owner strings are stable")
{
    CHECK(AnalysisRuntimeLock::owner_to_string(AnalysisRuntimeLock::Owner::Gui) == "gui");
    CHECK(AnalysisRuntimeLock::owner_to_string(AnalysisRuntimeLock::Owner::ExplorerWorker) ==
          "explorerWorker");
    CHECK(AnalysisRuntimeLock::owner_to_string(AnalysisRuntimeLock::Owner::Headless) == "headless");
    CHECK(AnalysisRuntimeLock::owner_from_string("gui") == AnalysisRuntimeLock::Owner::Gui);
    CHECK(AnalysisRuntimeLock::owner_from_string("explorerWorker") ==
          AnalysisRuntimeLock::Owner::ExplorerWorker);
    CHECK(AnalysisRuntimeLock::owner_from_string("headless") == AnalysisRuntimeLock::Owner::Headless);
    CHECK(AnalysisRuntimeLock::owner_from_string("unexpected") == AnalysisRuntimeLock::Owner::Unknown);
}
