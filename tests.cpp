#include <iostream>
#include <vector>
#include <chrono>
#include <cassert>
#include <memory>
#include <type_traits>
#include <utility>

#include "UnqPtr.h"
#include "ShrdPtr.h"
#include "Stack.h"

static_assert(!std::is_copy_constructible_v<UnqPtr<int>>);
static_assert(!std::is_copy_assignable_v<UnqPtr<int>>);
static_assert(std::is_nothrow_move_constructible_v<UnqPtr<int>>);
static_assert(std::is_nothrow_move_assignable_v<UnqPtr<int>>);

static_assert(!std::is_copy_constructible_v<UnqPtr<int[]>>);
static_assert(!std::is_copy_assignable_v<UnqPtr<int[]>>);
static_assert(std::is_nothrow_move_constructible_v<UnqPtr<int[]>>);
static_assert(std::is_nothrow_move_assignable_v<UnqPtr<int[]>>);

struct Object {
    inline static int alive = 0;
    int value;

    Object(int value = 0) : value(value) {
        ++alive;
    }

    Object(const Object& other) : value(other.value) {
        ++alive;
    }

    ~Object() {
        --alive;
    }
};

template <typename Pointer>
void benchmark(const char* name, std::size_t n,
               std::size_t extraMemory = 0) {
    auto start = std::chrono::steady_clock::now();

    long long sum = 0;

    {
        std::vector<Pointer> pointers(n);

        for (std::size_t i = 0; i < n; ++i) {
            pointers[i] = Pointer(new int(10));
        }

        for (std::size_t i = 0; i < n; ++i) {
            sum += *pointers[i];
        }
    } // Умные указатели автоматически удаляют объекты.

    auto finish = std::chrono::steady_clock::now();

    double time =
            std::chrono::duration<double, std::milli>(finish - start).count();

    double memory =
            n * (sizeof(Pointer) + sizeof(int) + extraMemory)
            / (1024.0 * 1024.0);

    std::cout << name << ": время = " << time << " мс";

    if constexpr (std::is_same_v<Pointer, std::shared_ptr<int>>) {
        std::cout << ", память: не менее " << memory
                  << " МиБ (без служебных блоков)";
    } else {
        std::cout << ", оценка памяти = " << memory << " МиБ";
    }

    std::cout << ", сумма = " << sum << '\n';
}

int main() {
    // Проверка UnqPtr.
    {
        UnqPtr<int> a(new int(5));
        assert(*a == 5);

        a.reset(new int(7));
        assert(*a == 7);
    }

    // Проверка ShrdPtr.
    {
        ShrdPtr<int> a(new int(10));
        ShrdPtr<int> b(a);

        assert(a.use_count() == 2);

        *b = 20;
        assert(*a == 20);

        b.reset();
        assert(a.use_count() == 1);
    }

    std::cout << "Проверки пройдены\n";

    std::size_t n = 100000;

    benchmark<UnqPtr<int>>("UnqPtr", n);
    benchmark<std::unique_ptr<int>>("std::unique_ptr", n);

    benchmark<ShrdPtr<int>>("ShrdPtr", n, sizeof(std::size_t));
    benchmark<std::shared_ptr<int>>("std::shared_ptr", n);

    {
        UnqPtr<Object> a(new Object(5));
        UnqPtr<Object> b(std::move(a));

        assert(a.get() == nullptr);
        assert(b->value == 5);

        b.reset(new Object(7));
        assert(b->value == 7);
        assert(Object::alive == 1);
    }
    assert(Object::alive == 0);

    // ShrdPtr: копирование и совместное владение.
    {
        ShrdPtr<Object> a(new Object(10));
        ShrdPtr<Object> b(a);

        assert(a.use_count() == 2);

        b->value = 20;
        assert(a->value == 20);

        a.reset();
        assert(b.use_count() == 1);
        assert(b->value == 20);
    }
    assert(Object::alive == 0);

    // UnqPtr для массива.
    {
        UnqPtr<Object[]> a(new Object[3]);
        a[2].value = 30;

        assert(a[2].value == 30);
        assert(Object::alive == 3);
    }
    assert(Object::alive == 0);

    // ShrdPtr для массива.
    {
        ShrdPtr<Object[]> a(new Object[3]);
        ShrdPtr<Object[]> b(a);

        a.reset();
        assert(b.use_count() == 1);
        assert(Object::alive == 3);
    }
    assert(Object::alive == 0);

    // UnqPtr: присваивание и обмен.
    {
        UnqPtr<Object> a(new Object(10));
        UnqPtr<Object> b(new Object(20));
        a = std::move(b);
        assert(a->value == 20 && b.get() == nullptr);
        assert(Object::alive == 1);

        a = std::move(a);
        assert(a->value == 20);
        a.swap(a);
        assert(a->value == 20);

        b.reset(new Object(30));
        a.swap(b);
        assert(a->value == 30 && b->value == 20);
    }
    assert(Object::alive == 0);

    // ShrdPtr: присваивание занятому указателю.
    {
        ShrdPtr<Object> a(new Object(10));
        ShrdPtr<Object> b(new Object(20));
        a = b;
        assert(a.get() == b.get() && a.use_count() == 2);
        assert(Object::alive == 1);

        ShrdPtr<Object> c(new Object(30));
        c = std::move(b);
        assert(c.get() == a.get() && c.use_count() == 2);
        assert(b.get() == nullptr && b.use_count() == 0);
        assert(Object::alive == 1);
    }
    assert(Object::alive == 0);

    // Пустые указатели.
    {
        UnqPtr<int> a;
        a.reset();
        UnqPtr<int> b(std::move(a));
        assert(a.get() == nullptr && b.get() == nullptr);
        a.reset(new int(10));
        a.swap(b);
        assert(a.get() == nullptr && *b == 10);
        b = std::move(a);
        assert(b.get() == nullptr);

        ShrdPtr<int> c;
        c.reset();
        ShrdPtr<int> d(c);
        ShrdPtr<int> e(std::move(c));
        assert(c.get() == nullptr && c.use_count() == 0);
        assert(d.get() == nullptr && d.use_count() == 0);
        assert(e.get() == nullptr && e.use_count() == 0);
        c.reset(new int(20));
        c.swap(d);
        assert(c.get() == nullptr && c.use_count() == 0);
        assert(*d == 20 && d.use_count() == 1);
        d = c;
        assert(d.get() == nullptr && d.use_count() == 0);
    }

    // ShrdPtr: самоприсваивание и обмен.
    {
        ShrdPtr<Object> a(new Object(10));
        ShrdPtr<Object> b(new Object(20));
        ShrdPtr<Object> c(a);
        a = a;
        assert(a.get() == c.get() && a.use_count() == 2);
        a = std::move(a);
        assert(a.get() == c.get() && a.use_count() == 2);
        a.swap(a);
        assert(a.get() == c.get() && a.use_count() == 2);

        a.swap(b);
        assert(a->value == 20 && a.use_count() == 1);
        assert(b.get() == c.get() && b.use_count() == 2);
        assert(c->value == 10 && c.use_count() == 2);
        assert(Object::alive == 2);
    }
    assert(Object::alive == 0);

    // Stack: порядок элементов, изменение вершины, повторное заполнение.
    {
        Stack<int> a;
        a.push(10);
        a.push(20);
        assert(a.top() == 20);
        a.top() = 30;
        assert(a.top() == 30);
        a.pop();
        assert(a.top() == 10);
        a.pop();
        a.push(40);
        assert(a.top() == 40);
    }

    // Stack: удаление элемента и уничтожение длинной цепочки.
    {
        Stack<Object> a;
        for (int i = 0; i < 100000; ++i) {
            a.push(Object(i));
        }
        assert(Object::alive == 100000);
        a.pop();
        assert(Object::alive == 99999);
        assert(a.top().value == 99998);
    }
    assert(Object::alive == 0);

    std::cout << "Все проверки пройдены\n";
}
