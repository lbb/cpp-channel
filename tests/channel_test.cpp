#include "channel.hpp"

#include <atomic>
#include <string>
#include <thread>
#include <vector>

#include "gtest/gtest.h"

TEST(ChannelTest, PushAndFetch)
{
    Channel<int> channel;

    int in = 1;
    in >> channel;
    in = 2;
    in >> channel;

    int out;
    out << channel;
    EXPECT_EQ(1, out);

    out << channel;
    EXPECT_EQ(2, out);
}

TEST(ChannelTest, PushByMoveAndFetch)
{
    Channel<std::string> channel;

    std::string in = "abc";
    std::move(in) >> channel;

    std::string{"def"} >> channel;

    std::string out;
    out << channel;
    EXPECT_EQ("abc", out);

    out << channel;
    EXPECT_EQ("def", out);
}

TEST(ChannelTest, size)
{
    Channel<int> channel;
    EXPECT_EQ(0, channel.size());

    int in = 1;
    in >> channel;
    EXPECT_EQ(1, channel.size());

    in << channel;
    EXPECT_EQ(0, channel.size());
}

TEST(ChannelTest, empty)
{
    Channel<int> channel;
    EXPECT_TRUE(channel.empty());

    int in = 1;
    in >> channel;
    EXPECT_FALSE(channel.empty());

    in << channel;
    EXPECT_TRUE(channel.empty());
}

TEST(ChannelTest, close)
{
    Channel<int> channel;
    EXPECT_FALSE(channel.closed());

    int in = 1;
    in >> channel;

    channel.close();
    EXPECT_TRUE(channel.closed());

    int out = 0;
    out << channel;
    EXPECT_EQ(1, out);
    EXPECT_NO_THROW(out << channel);

    EXPECT_THROW(in >> channel, ClosedChannel);
    EXPECT_THROW(std::move(in) >> channel, ClosedChannel);
}

TEST(ChannelTest, Iterator)
{
    Channel<int> channel;

    int in = 1;
    in >> channel;

    for (auto it = channel.begin(); it != channel.end();) {
        EXPECT_EQ(1, *it);
        break;
    }
}

TEST(ChannelTest, Multithreading)
{
    const int numbers = 100000;
    const long long expected = 5000050000;
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

    auto worker = [&] {
        // Wait until there is data on the channel
        std::unique_lock<std::mutex> lock{mtx_read};
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
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < threads_to_read_from; ++i) {
        auto th = std::thread{worker};
        threads.emplace_back(std::move(th));
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
    std::unique_lock<std::mutex> lock{mtx_wait};
    cond_wait.wait(lock, [&wait_counter]() {
        auto items = wait_counter.load();
        return items == 0;
    });

    std::for_each(threads.begin(), threads.end(), [](std::thread& thread) { thread.join(); });

    EXPECT_EQ(expected, sum_numbers);
}