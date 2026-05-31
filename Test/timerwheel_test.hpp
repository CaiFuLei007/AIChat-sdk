
#pragma once

#include <gtest/gtest.h>
#include "base/timerwheel.h"
#include "base/util/mylog.h"
#include <chrono>
#include <thread>
#include <atomic>
#include <vector>
#include <mutex>

class TimerWheelTest : public ::testing::Test
{
protected:
    TimerWheel* timer_wheel_;

    static void SetUpTestSuite()
    {
        ai_sdk::Logger::initLogger("test", "stdout", spdlog::level::debug);
    }

    void SetUp() override
    {
        timer_wheel_ = new TimerWheel();
        timer_wheel_->Ready();  // 启动定时器

        // 等待一小段时间确保定时器线程启动
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    void TearDown() override
    {
        delete timer_wheel_;
        timer_wheel_ = nullptr;
    }
};

// =================================================================
//                         基础功能测试
// =================================================================

// 测试 TimerWheel 能否正常创建
TEST_F(TimerWheelTest, CreateTimerWheel)
{
    EXPECT_NE(timer_wheel_, nullptr);
}

// 测试定时任务是否在指定时间后执行
TEST_F(TimerWheelTest, TaskExecutesAfterTimeout)
{
    std::atomic<bool> executed{false};
    auto start_time = std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point execution_time;

    timer_wheel_->SetTask(1, 2, [&executed, &execution_time]() {
        execution_time = std::chrono::steady_clock::now();
        executed = true;
    });

    // 等待任务执行（2秒超时 + 1秒余量）
    std::this_thread::sleep_for(std::chrono::seconds(4));

    ASSERT_TRUE(executed) << "任务应该在2秒后执行";

    // 验证执行时间精度
    auto actual_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        execution_time - start_time
    ).count();

    // 允许 1.5秒 到 3.5秒 的范围（考虑定时器精度）
    EXPECT_GE(actual_duration, 1500) << "任务执行时间不应该太早";
    EXPECT_LE(actual_duration, 3500) << "任务执行时间不应该太晚";
}

// 测试超时为1秒的任务
TEST_F(TimerWheelTest, TaskExecutesAfter1Second)
{
    std::atomic<bool> executed{false};

    timer_wheel_->SetTask(1, 1, [&executed]() {
        executed = true;
    });

    // 等待 0.5 秒，任务不应该执行
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    EXPECT_FALSE(executed) << "任务不应该在1秒前执行";

    // 再等待 1.5 秒，任务应该执行
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    EXPECT_TRUE(executed) << "任务应该在1秒后执行";
}

// 测试多个任务按超时顺序执行
TEST_F(TimerWheelTest, MultipleTasksExecuteInOrder)
{
    std::vector<int> execution_order;
    std::mutex mutex;

    // 设置3个不同超时的任务
    timer_wheel_->SetTask(1, 1, [&execution_order, &mutex]() {
        std::lock_guard<std::mutex> lock(mutex);
        execution_order.push_back(1);
    });

    timer_wheel_->SetTask(2, 2, [&execution_order, &mutex]() {
        std::lock_guard<std::mutex> lock(mutex);
        execution_order.push_back(2);
    });

    timer_wheel_->SetTask(3, 3, [&execution_order, &mutex]() {
        std::lock_guard<std::mutex> lock(mutex);
        execution_order.push_back(3);
    });

    // 等待所有任务执行完成
    std::this_thread::sleep_for(std::chrono::seconds(5));

    ASSERT_EQ(execution_order.size(), 3);
    EXPECT_EQ(execution_order[0], 1);
    EXPECT_EQ(execution_order[1], 2);
    EXPECT_EQ(execution_order[2], 3);
}

// 测试同一时刻多个任务都能执行
TEST_F(TimerWheelTest, MultipleTasksWithSameTimeout)
{
    std::atomic<int> execution_count{0};
    const int num_tasks = 10;

    // 设置多个相同超时的任务
    for (int i = 0; i < num_tasks; ++i) {
        timer_wheel_->SetTask(i, 2, [&execution_count]() {
            execution_count++;
        });
    }

    // 等待任务执行
    std::this_thread::sleep_for(std::chrono::seconds(4));

    EXPECT_EQ(execution_count, num_tasks) << "所有任务都应该执行";
}

// =================================================================
//                         取消任务测试
// =================================================================

// 测试在任务执行前取消
TEST_F(TimerWheelTest, CancelTaskBeforeExecution)
{
    std::atomic<bool> executed{false};

    timer_wheel_->SetTask(1, 3, [&executed]() {
        executed = true;
    });

    // 在任务执行前取消
    std::this_thread::sleep_for(std::chrono::seconds(1));
    timer_wheel_->CancelTask(1);

    // 等待原本的超时时间
    std::this_thread::sleep_for(std::chrono::seconds(4));

    EXPECT_FALSE(executed) << "已取消的任务不应该执行";
}

// 测试取消不存在的任务不会崩溃
TEST_F(TimerWheelTest, CancelNonExistentTask)
{
    EXPECT_NO_THROW(timer_wheel_->CancelTask(999));
}

// 测试取消已执行的任务不会崩溃
TEST_F(TimerWheelTest, CancelAlreadyExecutedTask)
{
    std::atomic<bool> executed{false};

    timer_wheel_->SetTask(1, 1, [&executed]() {
        executed = true;
    });

    // 等待任务执行
    std::this_thread::sleep_for(std::chrono::seconds(3));
    ASSERT_TRUE(executed);

    // 取消已执行的任务不应该崩溃
    EXPECT_NO_THROW(timer_wheel_->CancelTask(1));
}

// =================================================================
//                         更新任务测试（延长生命周期）
// =================================================================

// 测试 UpdateTask 延长任务生命周期
// UpdateTask 会将 shared_ptr 再次添加到时间轮，延长任务的生命周期
TEST_F(TimerWheelTest, UpdateTaskExtendsLifetime)
{
    std::atomic<bool> executed{false};

    // 设置一个2秒超时的任务
    timer_wheel_->SetTask(1, 2, [&executed]() {
        executed = true;
    });

    // 等待1秒后更新任务（添加新的 shared_ptr 引用）
    std::this_thread::sleep_for(std::chrono::seconds(1));
    timer_wheel_->UpdateTask(1);

    // 原本2秒后应该执行，但由于 UpdateTask 添加了新引用
    // 任务会在所有 shared_ptr 都释放后才执行
    // 第一个引用在 tick=2 时释放，第二个引用在 tick=3 时释放
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // 此时第一个 shared_ptr 已释放，但第二个还在
    // 任务可能还没执行（取决于时间轮的 tick 位置）

    // 再等待2秒，确保所有引用都释放
    std::this_thread::sleep_for(std::chrono::seconds(2));

    EXPECT_TRUE(executed) << "任务最终应该执行";
}

// 测试多次 UpdateTask
TEST_F(TimerWheelTest, MultipleUpdatesExtendLifetime)
{
    std::atomic<bool> executed{false};

    // 设置一个1秒超时的任务
    timer_wheel_->SetTask(1, 1, [&executed]() {
        executed = true;
    });

    // 连续更新3次，每次间隔500ms
    for (int i = 0; i < 3; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        timer_wheel_->UpdateTask(1);
    }

    // 等待足够长时间让所有 shared_ptr 释放
    std::this_thread::sleep_for(std::chrono::seconds(3));

    EXPECT_TRUE(executed) << "任务最终应该执行";
}

// 测试更新不存在的任务不会崩溃
TEST_F(TimerWheelTest, UpdateNonExistentTask)
{
    EXPECT_NO_THROW(timer_wheel_->UpdateTask(999));
}

// =================================================================
//                         重复ID测试
// =================================================================

// 测试用相同ID设置新任务会取消旧任务
TEST_F(TimerWheelTest, SetTaskWithSameIdReplacesOldTask)
{
    std::atomic<int> first_task_executed{0};
    std::atomic<int> second_task_executed{0};

    // 设置第一个任务
    timer_wheel_->SetTask(1, 3, [&first_task_executed]() {
        first_task_executed++;
    });

    // 用相同ID设置第二个任务（应该取消第一个）
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    timer_wheel_->SetTask(1, 2, [&second_task_executed]() {
        second_task_executed++;
    });

    // 等待任务执行
    std::this_thread::sleep_for(std::chrono::seconds(4));

    EXPECT_EQ(first_task_executed, 0) << "旧任务应该被取消";
    EXPECT_EQ(second_task_executed, 1) << "新任务应该执行";
}

// =================================================================
//                         并发安全测试
// =================================================================

// 测试并发设置任务
TEST_F(TimerWheelTest, ConcurrentSetTasks)
{
    std::atomic<int> execution_count{0};
    const int num_tasks = 20;

    // 并发设置多个任务
    std::vector<std::thread> threads;
    for (int i = 0; i < num_tasks; ++i) {
        threads.emplace_back([this, i, &execution_count]() {
            timer_wheel_->SetTask(i, 2, [&execution_count]() {
                execution_count++;
            });
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // 等待所有任务执行
    std::this_thread::sleep_for(std::chrono::seconds(4));

    EXPECT_EQ(execution_count, num_tasks) << "所有任务都应该执行";
}

// 测试并发取消任务
TEST_F(TimerWheelTest, ConcurrentCancelTasks)
{
    const int num_tasks = 10;

    // 先设置任务
    for (int i = 0; i < num_tasks; ++i) {
        timer_wheel_->SetTask(i, 5, []() {});
    }

    // 并发取消任务
    std::vector<std::thread> threads;
    for (int i = 0; i < num_tasks; ++i) {
        threads.emplace_back([this, i]() {
            timer_wheel_->CancelTask(i);
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // 不应该崩溃
    SUCCEED();
}

// 测试并发更新任务
TEST_F(TimerWheelTest, ConcurrentUpdateTasks)
{
    const int num_tasks = 10;

    // 先设置任务
    for (int i = 0; i < num_tasks; ++i) {
        timer_wheel_->SetTask(i, 5, []() {});
    }

    // 并发更新任务
    std::vector<std::thread> threads;
    for (int i = 0; i < num_tasks; ++i) {
        threads.emplace_back([this, i]() {
            timer_wheel_->UpdateTask(i);
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // 不应该崩溃
    SUCCEED();
}

// 测试并发混合操作
TEST_F(TimerWheelTest, ConcurrentMixedOperations)
{
    std::atomic<int> execution_count{0};
    const int num_tasks = 30;

    std::vector<std::thread> threads;

    // 一些线程设置任务
    for (int i = 0; i < num_tasks; ++i) {
        threads.emplace_back([this, i, &execution_count]() {
            timer_wheel_->SetTask(i, 3, [&execution_count]() {
                execution_count++;
            });
        });
    }

    // 一些线程取消任务
    for (int i = 0; i < num_tasks / 3; ++i) {
        threads.emplace_back([this, i]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            timer_wheel_->CancelTask(i);
        });
    }

    // 一些线程更新任务
    for (int i = num_tasks / 3; i < num_tasks * 2 / 3; ++i) {
        threads.emplace_back([this, i]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            timer_wheel_->UpdateTask(i);
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // 等待任务执行
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // 不应该崩溃，且至少有一些任务执行
    EXPECT_GT(execution_count, 0);
}

// =================================================================
//                         边界条件测试
// =================================================================

// 测试超时值为0的任务（下一个tick执行）
TEST_F(TimerWheelTest, ZeroTimeoutTask)
{
    std::atomic<bool> executed{false};

    timer_wheel_->SetTask(1, 0, [&executed]() {
        executed = true;
    });

    // 等待下一个tick
    std::this_thread::sleep_for(std::chrono::seconds(2));

    EXPECT_TRUE(executed) << "超时为0的任务应该在当前tick执行";
}

// 测试超时值超过时间轮大小（取模后执行）
TEST_F(TimerWheelTest, TimeoutExceedsWheelSize)
{
    std::atomic<bool> executed{false};

    // 时间轮大小为60，设置超时为65
    // 65 % 60 = 5，所以实际在第5个槽位
    timer_wheel_->SetTask(1, 65, [&executed]() {
        executed = true;
    });

    // 等待6秒（5秒 + 1秒余量）
    std::this_thread::sleep_for(std::chrono::seconds(7));

    EXPECT_TRUE(executed) << "超时65秒的任务应该在5秒后执行（取模）";
}

// 测试 Lambda 捕获值正确传递
TEST_F(TimerWheelTest, TaskWithLambdaCapture)
{
    int captured_value = 42;
    std::atomic<int> result{0};

    timer_wheel_->SetTask(1, 1, [captured_value, &result]() {
        result = captured_value;
    });

    std::this_thread::sleep_for(std::chrono::seconds(3));

    EXPECT_EQ(result, 42) << "Lambda捕获的值应该正确传递";
}

// 测试空 Lambda 不会崩溃
TEST_F(TimerWheelTest, EmptyLambdaTask)
{
    timer_wheel_->SetTask(1, 1, []() {});

    std::this_thread::sleep_for(std::chrono::seconds(2));

    SUCCEED();
}

// =================================================================
//                         定时精度测试
// =================================================================

// 测试定时精度（多个任务）
TEST_F(TimerWheelTest, TimingAccuracyMultipleTasks)
{
    struct TaskRecord {
        int id;
        int expected_timeout;
        std::chrono::steady_clock::time_point execution_time;
        bool executed = false;
    };

    std::vector<TaskRecord> records(5);
    std::mutex mutex;
    auto start_time = std::chrono::steady_clock::now();

    for (int i = 0; i < 5; ++i) {
        records[i].id = i;
        records[i].expected_timeout = i + 1;  // 1, 2, 3, 4, 5 秒

        timer_wheel_->SetTask(i, i + 1, [&records, &mutex, i, start_time]() {
            std::lock_guard<std::mutex> lock(mutex);
            records[i].execution_time = std::chrono::steady_clock::now();
            records[i].executed = true;
        });
    }

    // 等待所有任务执行
    std::this_thread::sleep_for(std::chrono::seconds(7));

    for (const auto& record : records) {
        ASSERT_TRUE(record.executed) << "任务 " << record.id << " 应该执行";

        auto actual_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            record.execution_time - start_time
        ).count();

        int expected_ms = record.expected_timeout * 1000;

        // 允许 ±1秒 的误差
        EXPECT_GE(actual_ms, expected_ms - 1000)
            << "任务 " << record.id << " 执行太早";
        EXPECT_LE(actual_ms, expected_ms + 1000)
            << "任务 " << record.id << " 执行太晚";
    }
}

// =================================================================
//                         复杂场景测试
// =================================================================

// 回调已移出临界区执行，在回调中调用 SetTask/UpdateTask/CancelTask 不再死锁
TEST_F(TimerWheelTest, SetTaskInCallback)
{
    std::atomic<int> execution_order{0};

    timer_wheel_->SetTask(1, 1, [this, &execution_order]() {
        execution_order = 1;
        timer_wheel_->SetTask(2, 1, [&execution_order]() {
            execution_order = 2;
        });
    });

    // 等待两个任务都执行
    std::this_thread::sleep_for(std::chrono::seconds(4));

    EXPECT_EQ(execution_order, 2) << "链式任务应该按顺序执行";
}

// 测试混合操作：设置、取消、更新
TEST_F(TimerWheelTest, MixedOperations)
{
    std::atomic<int> executed_count{0};

    // 设置10个任务
    for (int i = 0; i < 10; ++i) {
        timer_wheel_->SetTask(i, 2, [&executed_count]() {
            executed_count++;
        });
    }

    // 取消3个任务
    timer_wheel_->CancelTask(0);
    timer_wheel_->CancelTask(2);
    timer_wheel_->CancelTask(4);

    // 更新2个任务（延长生命周期）
    timer_wheel_->UpdateTask(1);
    timer_wheel_->UpdateTask(3);

    // 等待任务执行
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // 应该有7个任务执行（10 - 3个取消的）
    EXPECT_EQ(executed_count, 7) << "未取消的任务应该执行";
}

// =================================================================
//                         性能测试
// =================================================================

// 测试大量任务创建性能
TEST_F(TimerWheelTest, HighVolumeTaskCreation)
{
    const int num_tasks = 1000;
    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < num_tasks; ++i) {
        timer_wheel_->SetTask(i, 30, []() {});
    }

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // 创建1000个任务应该在1秒内完成
    EXPECT_LT(duration.count(), 1000) << "任务创建应该很快";

    // 取消所有任务避免等待
    for (int i = 0; i < num_tasks; ++i) {
        timer_wheel_->CancelTask(i);
    }
}

// 测试快速设置和取消
TEST_F(TimerWheelTest, RapidSetAndCancel)
{
    for (int i = 0; i < 100; ++i) {
        timer_wheel_->SetTask(1, 10, []() {});
        timer_wheel_->CancelTask(1);
    }

    // 不应该崩溃
    SUCCEED();
}
