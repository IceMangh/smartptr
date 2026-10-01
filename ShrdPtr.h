#ifndef SMARTPTR_SHRDPTR_H
#define SMARTPTR_SHRDPTR_H

#include <cstddef>
#include <utility>

template <typename T>
class ShrdPtr{
private:
    T* ptr;
    unsigned long* referenceCount;

public:
    ShrdPtr(T* obj): ptr(obj), referenceCount(nullptr){
        if (obj != nullptr) {
            try {
                referenceCount = new std::size_t(1);
            } catch (...) {
                delete ptr;
                throw;
            }
        }
    }

    ShrdPtr(): ptr(nullptr), referenceCount(nullptr){}

    ShrdPtr(const ShrdPtr& other) noexcept {
        ptr = other.ptr;
        referenceCount = other.referenceCount;
        if(referenceCount != nullptr) {
            (*referenceCount)++;
        }
    }

    ShrdPtr(ShrdPtr&& other) noexcept {
        ptr = other.ptr;
        referenceCount = other.referenceCount;
        other.ptr = nullptr;
        other.referenceCount = nullptr;
    }

    ~ShrdPtr(){
        if (referenceCount != nullptr){
            (*referenceCount)--;
            if (*referenceCount == 0){
                delete ptr;
                delete referenceCount;
            }
        }
    }

    void swap(ShrdPtr& other) noexcept {
        T* temp = ptr;
        ptr = other.ptr;
        other.ptr = temp;

        std::size_t* countTemp = referenceCount;
        referenceCount = other.referenceCount;
        other.referenceCount = countTemp;
    }

    ShrdPtr& operator=(const ShrdPtr& other) noexcept {
        ShrdPtr temp(other);

        this->swap(temp);

        return *this;
    }

    ShrdPtr& operator=(ShrdPtr&& other) noexcept {
        ShrdPtr temp(std::move(other));

        this->swap(temp);

        return *this;
    }

    T& operator*() const {
        return *ptr;
    }

    T* operator->() const {
        return ptr;
    }

    T* get() const {
        return ptr;
    }

     unsigned long use_count() const noexcept {
        if (referenceCount == nullptr){
            return 0;
        } else {
            return *referenceCount;
        }
    }

    void reset() noexcept {
        ShrdPtr temp;

        this->swap(temp);
    }

    void reset(T* obj) {
        if (obj == ptr) {
            return;
        }

        ShrdPtr temp(obj);

        this->swap(temp);
    }
};

template <typename T>
class ShrdPtr<T[]> {
private:
    T* ptr;
    std::size_t* referenceCount;

public:
    explicit ShrdPtr(T* obj)
            : ptr(obj), referenceCount(nullptr) {
        if (obj != nullptr) {
            try {
                referenceCount = new std::size_t(1);
            } catch (...) {
                delete[] ptr;
                throw;
            }
        }
    }

    ShrdPtr() noexcept
            : ptr(nullptr), referenceCount(nullptr) {}

    ShrdPtr(const ShrdPtr& other) noexcept
            : ptr(other.ptr), referenceCount(other.referenceCount) {
        if (referenceCount != nullptr) {
            (*referenceCount)++;
        }
    }

    ShrdPtr(ShrdPtr&& other) noexcept
            : ptr(other.ptr), referenceCount(other.referenceCount) {
        other.ptr = nullptr;
        other.referenceCount = nullptr;
    }

    ~ShrdPtr() {
        if (referenceCount != nullptr) {
            (*referenceCount)--;

            if (*referenceCount == 0) {
                delete[] ptr;
                delete referenceCount;
            }
        }
    }

    void swap(ShrdPtr& other) noexcept {
        T* temp = ptr;
        ptr = other.ptr;
        other.ptr = temp;

        std::size_t* countTemp = referenceCount;
        referenceCount = other.referenceCount;
        other.referenceCount = countTemp;
    }

    ShrdPtr& operator=(const ShrdPtr& other) noexcept {
        ShrdPtr temp(other);
        swap(temp);
        return *this;
    }

    ShrdPtr& operator=(ShrdPtr&& other) noexcept {
        ShrdPtr temp(std::move(other));
        swap(temp);
        return *this;
    }

    T& operator[](std::size_t index) const noexcept {
        return ptr[index];
    }

    T* get() const noexcept {
        return ptr;
    }

    std::size_t use_count() const noexcept {
        if (referenceCount == nullptr) {
            return 0;
        }

        return *referenceCount;
    }

    void reset() noexcept {
        ShrdPtr temp;
        swap(temp);
    }

    void reset(T* obj) {
        if (obj == ptr) {
            return;
        }

        ShrdPtr temp(obj);
        swap(temp);
    }
};

#endif //SMARTPTR_SHRDPTR_H
