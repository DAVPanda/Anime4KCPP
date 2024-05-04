#ifndef AC_UTIL_CHANNEL_HPP
#define AC_UTIL_CHANNEL_HPP

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <queue>
#include <type_traits>
#include <utility>

namespace ac::util
{
    template<typename Queue, typename = void>
    constexpr bool HasTopFunction = false;
    template<typename Queue>
    constexpr bool HasTopFunction<Queue, std::void_t<decltype(std::declval<Queue>().top())>> = true;

    template <typename T, typename Queue>
    class Channel;

    template<typename T>
    using AscendingChannel = Channel<T, std::priority_queue<T, std::vector<T>, std::greater<T>>>;
    template<typename T>
    using DescendingChannel = Channel<T, std::priority_queue<T, std::vector<T>, std::less<T>>>;
}

template <typename T, typename Queue = std::queue<T>>
class ac::util::Channel
{
public:
    explicit Channel(std::size_t capacity);
    Channel(const Channel<T, Queue>&) = delete;
    Channel(Channel<T, Queue>&&) = delete;
    Channel<T, Queue>& operator=(const Channel<T, Queue>&) = delete;
    Channel<T, Queue>& operator=(Channel<T, Queue>&&) = delete;
    ~Channel() = default;

    Channel<T, Queue>& operator<<(const T& obj);
    Channel<T, Queue>& operator>>(T& obj);
    std::size_t size();
    bool empty();
    void close();
    bool isClose();
private:
    bool stop;
    const std::size_t capacity;
    std::mutex mtx{};
    std::condition_variable consumer{}, producer{};
    Queue queue{};
};

template<typename T, typename Queue>
inline ac::util::Channel<T, Queue>::Channel(const std::size_t capacity) : stop(false), capacity(capacity) {}
template<typename T, typename Queue>
inline ac::util::Channel<T, Queue>& ac::util::Channel<T, Queue>::operator<<(const T& obj)
{
    std::unique_lock<std::mutex> lock(mtx);
    producer.wait(lock, [&](){ return queue.size() < capacity; });
    queue.emplace(obj);
    lock.unlock();
    consumer.notify_one();

    return *this;
}
template<typename T, typename Queue>
inline ac::util::Channel<T, Queue>& ac::util::Channel<T, Queue>::operator>>(T& obj)
{
    std::unique_lock<std::mutex> lock(mtx);
    consumer.wait(lock, [&](){ return stop || !queue.empty(); });
    if (!queue.empty())
    {
        if constexpr (HasTopFunction<Queue>) obj = queue.top();
        else obj = std::move(queue.front());
        queue.pop();
    }
    lock.unlock();
    producer.notify_one();

    return *this;
}
template<typename T, typename Queue>
inline std::size_t ac::util::Channel<T, Queue>::size()
{
    const std::lock_guard<std::mutex> lock(mtx);
    return queue.size();
}
template<typename T, typename Queue>
inline bool ac::util::Channel<T, Queue>::empty()
{
    const std::lock_guard<std::mutex> lock(mtx);
    return queue.empty();
}
template<typename T, typename Queue>
inline void ac::util::Channel<T, Queue>::close()
{
    {
        const std::lock_guard<std::mutex> lock(mtx);
        stop = true;
    }
    consumer.notify_all();
}
template<typename T, typename Queue>
inline bool ac::util::Channel<T, Queue>::isClose()
{
    const std::lock_guard<std::mutex> lock(mtx);
    return stop;
}

#endif
