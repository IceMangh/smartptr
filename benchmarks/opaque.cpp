// A separate translation unit, without LTO, keeps ownership operations observable.
// The barrier does not dereference the object or add work per pointer.
extern "C" void observe_pointer(const void* address) {
#if defined(__clang__) || defined(__GNUC__)
    asm volatile("" : : "r"(address) : "memory");
#else
#error "The benchmark requires Clang or GCC for the compiler barrier"
#endif
}
