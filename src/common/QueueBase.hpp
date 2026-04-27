#pragma once

#include <cstddef>
#include <functional>

namespace cmf
{
template <typename Q, typename T>
concept QueueType = requires(Q& q, const Q& cq, T value) {
    q.push(std::move(value));
    {
        q.pop(std::declval<std::function<void(T&&)>>())
    } -> std::convertible_to<bool>;
    q.close();
    { cq.is_closed() } -> std::convertible_to<bool>;
    { cq.empty() } -> std::convertible_to<bool>;
    { cq.size() } -> std::convertible_to<std::size_t>;
};

template <typename Derived, typename T>
class QueueBase
{
  public:
    using value_type = T;

    QueueBase() = default;
    QueueBase(const QueueBase&) = delete;
    QueueBase& operator=(const QueueBase&) = delete;
    QueueBase(QueueBase&&) = delete;
    QueueBase& operator=(QueueBase&&) = delete;
    ~QueueBase() = default;

    void push(T value) { derived().push_impl(std::move(value)); }

    void push_batch(T* items, std::size_t count)
    {
        derived().push_batch_impl(items, count);
    }

    template <typename Fn>
        requires std::invocable<Fn, T&&>
    [[nodiscard]] bool pop(Fn&& fn)
    {
        return derived().pop_impl(std::forward<Fn>(fn));
    }

    void close() noexcept { derived().close_impl(); }

    [[nodiscard]] bool is_closed() const noexcept
    {
        return cderived().is_closed_impl();
    }
    [[nodiscard]] bool empty() const noexcept { return cderived().empty_impl(); }
    [[nodiscard]] std::size_t size() const noexcept
    {
        return cderived().size_impl();
    }

  private:
    Derived& derived() { return *static_cast<Derived*>(this); }
    const Derived& cderived() const
    {
        return *static_cast<const Derived*>(this);
    }
};
} // namespace cmf
