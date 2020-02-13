#include <thread>
#include <atomic>

#include "channel.h"

#include "gtest/gtest.h"

class ChannelTest : public ::testing::Test {
};

TEST_F(ChannelTest, test) {
    Channel<int> channel{};

    ASSERT_EQ(channel.size(), 0);

    int in = 2;
    in >> channel;
    in >> channel;
    ASSERT_EQ(channel.size(), 2);

    int out;
    out << channel;
    out << channel;
    ASSERT_EQ(out, in);
    ASSERT_EQ(channel.size(), 0);
}

TEST_F(ChannelTest, multithread) {
    const int numbers = 1000000 * 2;
    const long long expected = 2000001000000;
    const int threads_to_read_from = 100;

    Channel<int> channel{10};

    std::mutex mtx_read;
    std::condition_variable cond_read;
    bool ready_to_read{};
    std::atomic<long long> count_numbers{};
    std::atomic<long long> sum_numbers{};

    std::mutex mtx_wait;
    std::condition_variable cond_wait;
    std::atomic<int> wait_counter{};
    wait_counter = threads_to_read_from;

    for (int i = 0; i < threads_to_read_from; ++i) {
        (std::thread{
                [&channel, &mtx_read, i, &cond_read, &ready_to_read, &count_numbers, &sum_numbers, &wait_counter, &cond_wait] {
                    // Wait until there is data on the channel
                    std::unique_lock<std::mutex> lock(mtx_read);
                    cond_read.wait(lock, [&ready_to_read] { return ready_to_read; });

                    // Read until all items have been read from the channel
                    while (count_numbers < numbers) {
                        int out{};
                        out << channel;

                        sum_numbers += out;
                        ++count_numbers;
                    }
                    --wait_counter;
                    cond_wait.notify_one();
                }}).detach();
    }

    // Send numbers to channel
    for (int i = 1; i <= numbers; ++i) {
        i >> channel;

        // Notify threads than then can start reading
        if (!ready_to_read) {
            ready_to_read = true;
            cond_read.notify_all();
        }
    }

    // Wait until all items have been read
    std::unique_lock<std::mutex> lock(mtx_wait);
    cond_wait.wait(lock, [&wait_counter]() { return wait_counter == 0; });

    ASSERT_EQ(sum_numbers, expected);
}