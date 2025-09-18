#pragma once
#include <coroutine>
#include <optional>
#include <exception>

template <typename T>
struct Task
{
    struct promise_type
    {
        std::optional<T> value_;
        std::exception_ptr exception_;

        Task get_return_object()
        {
            return Task{
                std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        std::suspend_never initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }

        void return_value(T value) { value_ = std::move(value); }
        void unhandled_exception() { exception_ = std::current_exception(); }
    };

    std::coroutine_handle<promise_type> handle_;

    explicit Task(std::coroutine_handle<promise_type> h) : handle_(h) {}
    Task(const Task &) = delete;
    Task(Task &&other) : handle_(other.handle_) { other.handle_ = nullptr; }
    ~Task()
    {
        if (handle_)
            handle_.destroy();
    }

    // Phương thức đồng bộ: resume tới khi done và trả về giá trị
    T sync()
    {
        while (!handle_.done())
            handle_.resume();
        if (handle_.promise().exception_)
            std::rethrow_exception(handle_.promise().exception_);
        return std::move(handle_.promise().value_.value());
    }

    // Cho co_await bên trong coroutine khác
    bool await_ready() { return false; }
    void await_suspend(std::coroutine_handle<> h) { handle_.resume(); }
    T await_resume()
    {
        if (handle_.promise().exception_)
        {
            std::rethrow_exception(handle_.promise().exception_);
        }
        return std::move(handle_.promise().value_.value());
    }
};

// Task<void> specialization
template <>
struct Task<void>
{
    struct promise_type
    {
        std::exception_ptr exception_;

        Task get_return_object()
        {
            return Task{
                std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        std::suspend_never initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }

        void return_void() {}
        void unhandled_exception() { exception_ = std::current_exception(); }
    };

    std::coroutine_handle<promise_type> handle_;

    explicit Task(std::coroutine_handle<promise_type> h) : handle_(h) {}
    Task(const Task &) = delete;
    Task(Task &&other) : handle_(other.handle_) { other.handle_ = nullptr; }
    ~Task()
    {
        if (handle_)
            handle_.destroy();
    }

    void sync()
    {
        while (!handle_.done())
            handle_.resume();
        if (handle_.promise().exception_)
        {
            std::rethrow_exception(handle_.promise().exception_);
        }
    }

    bool await_ready() { return false; }
    void await_suspend(std::coroutine_handle<> h) { handle_.resume(); }
    void await_resume()
    {
        if (handle_.promise().exception_)
        {
            std::rethrow_exception(handle_.promise().exception_);
        }
    }
};
