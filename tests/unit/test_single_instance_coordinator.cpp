#include <catch2/catch_test_macros.hpp>

#include "SingleInstanceCoordinator.hpp"
#include "TestHelpers.hpp"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QThread>

namespace {

bool wait_for_condition(const std::function<bool()>& predicate, int timeout_ms = 1000)
{
    QElapsedTimer timer;
    timer.start();

    while (timer.elapsed() < timeout_ms) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 25);
        if (predicate()) {
            return true;
        }
        QThread::msleep(10);
    }

    QCoreApplication::processEvents(QEventLoop::AllEvents, 25);
    return predicate();
}

} // namespace

TEST_CASE("SingleInstanceCoordinator notifies the primary instance on relaunch")
{
    QtAppContext app_context;
    const QString instance_id = QString::fromStdString(make_unique_token("single-instance-"));

    SingleInstanceCoordinator primary(instance_id);
    bool activated = false;
    primary.set_activation_callback([&activated]() {
        activated = true;
    });

    REQUIRE(primary.acquire_primary_instance());
    REQUIRE(primary.is_primary_instance());

    SingleInstanceCoordinator secondary(instance_id);
    REQUIRE_FALSE(secondary.acquire_primary_instance());
    REQUIRE_FALSE(secondary.is_primary_instance());
    REQUIRE(wait_for_condition([&activated]() {
        return activated;
    }));
}

TEST_CASE("SingleInstanceCoordinator allows different instance ids to coexist")
{
    QtAppContext app_context;
    const QString first_id = QString::fromStdString(make_unique_token("single-instance-a-"));
    const QString second_id = QString::fromStdString(make_unique_token("single-instance-b-"));

    SingleInstanceCoordinator first(first_id);
    SingleInstanceCoordinator second(second_id);

    REQUIRE(first.acquire_primary_instance());
    REQUIRE(second.acquire_primary_instance());
    REQUIRE(first.is_primary_instance());
    REQUIRE(second.is_primary_instance());
}
