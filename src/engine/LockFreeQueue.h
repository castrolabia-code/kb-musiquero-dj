#pragma once
// =============================================================================
//  SpscQueue: cola lock-free de UN productor y UN consumidor, capacidad fija.
//
//  * Sin allocations después de construida, sin locks: segura para el hilo de
//    audio en cualquiera de los dos extremos.
//  * T debe ser trivialmente copiable (comandos, punteros).
//  * Capacidad potencia de 2; se usa una ranura menos que la capacidad.
// =============================================================================
#include <array>
#include <atomic>
#include <cstddef>
#include <type_traits>

#if defined (_MSC_VER)
 #pragma warning (push)
 #pragma warning (disable: 4324)   // relleno por alignas(64): es intencional (evita false sharing)
#endif

namespace kb::engine
{
    template <typename T, std::size_t Capacity>
    class SpscQueue
    {
        static_assert (Capacity >= 2 && (Capacity & (Capacity - 1)) == 0, "Capacity debe ser potencia de 2");
        static_assert (std::is_trivially_copyable_v<T>, "T debe ser trivialmente copiable");

    public:
        SpscQueue() = default;
        SpscQueue (const SpscQueue&) = delete;
        SpscQueue& operator= (const SpscQueue&) = delete;

        /** Productor. Devuelve false si la cola está llena (no bloquea). */
        bool push (const T& value) noexcept
        {
            const auto h    = head.load (std::memory_order_relaxed);
            const auto next = (h + 1) & mask;
            if (next == tail.load (std::memory_order_acquire))
                return false;

            slots[h] = value;
            head.store (next, std::memory_order_release);
            return true;
        }

        /** Consumidor. Devuelve false si la cola está vacía. */
        bool pop (T& out) noexcept
        {
            const auto t = tail.load (std::memory_order_relaxed);
            if (t == head.load (std::memory_order_acquire))
                return false;

            out = slots[t];
            tail.store ((t + 1) & mask, std::memory_order_release);
            return true;
        }

        bool isEmpty() const noexcept
        {
            return tail.load (std::memory_order_acquire) == head.load (std::memory_order_acquire);
        }

        static constexpr std::size_t capacity() noexcept { return Capacity - 1; }

    private:
        static constexpr std::size_t mask = Capacity - 1;

        std::array<T, Capacity> slots {};
        alignas (64) std::atomic<std::size_t> head { 0 };   // escribe el productor
        alignas (64) std::atomic<std::size_t> tail { 0 };   // escribe el consumidor
    };
} // namespace kb::engine

#if defined (_MSC_VER)
 #pragma warning (pop)
#endif
