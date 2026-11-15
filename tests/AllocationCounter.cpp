// Reemplaza el operator new/delete global del ejecutable de pruebas para poder
// contar allocations en el hilo de audio simulado (regla de tiempo real).
#include <atomic>
#include <cstdlib>
#include <new>

namespace kb::test
{
    thread_local bool countingAllocations = false;
    std::atomic<int>  allocationCount { 0 };
}

namespace
{
    void* allocate (std::size_t n)
    {
        if (kb::test::countingAllocations)
            kb::test::allocationCount.fetch_add (1);
        if (void* p = std::malloc (n > 0 ? n : 1))
            return p;
        throw std::bad_alloc();
    }

    void release (void* p) noexcept
    {
        if (p != nullptr && kb::test::countingAllocations)
            kb::test::allocationCount.fetch_add (1);
        std::free (p);
    }
}

void* operator new (std::size_t n)                  { return allocate (n); }
void* operator new[] (std::size_t n)                { return allocate (n); }
void  operator delete (void* p) noexcept            { release (p); }
void  operator delete[] (void* p) noexcept          { release (p); }
void  operator delete (void* p, std::size_t) noexcept   { release (p); }
void  operator delete[] (void* p, std::size_t) noexcept { release (p); }
