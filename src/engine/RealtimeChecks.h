#pragma once
// =============================================================================
//  Utilidades para hacer cumplir las reglas de tiempo real del motor.
//
//  Reglas (innegociables, ver docs/ARQUITECTURA.md):
//    * Cero allocations, locks, I/O de disco o logs dentro del callback.
//    * UI <-> motor solo con atómicos y colas lock-free.
// =============================================================================
#include <atomic>

namespace kb::engine
{
    // Todo valor compartido entre hilos debe usar atómicos libres de locks.
    // Si alguna plataforma no los garantiza, que falle la compilación en lugar
    // de que el audio se corte en vivo.
    static_assert (std::atomic<float>::is_always_lock_free,  "std::atomic<float> debe ser lock-free");
    static_assert (std::atomic<double>::is_always_lock_free, "std::atomic<double> debe ser lock-free");
    static_assert (std::atomic<int>::is_always_lock_free,    "std::atomic<int> debe ser lock-free");
    static_assert (std::atomic<bool>::is_always_lock_free,   "std::atomic<bool> debe ser lock-free");
    static_assert (std::atomic<unsigned long long>::is_always_lock_free, "std::atomic<uint64> debe ser lock-free");
} // namespace kb::engine
