#ifndef SMARTPTR_UNQPTR_H
#define SMARTPTR_UNQPTR_H

template <typename T>
class UnqPtr {
private:
    T* ptr;

public:
    UnqPtr(T* obj = nullptr): ptr(obj){}

    ~UnqPtr(){
        delete ptr;
    }

    UnqPtr(const UnqPtr<T>& other) = delete;

    UnqPtr& operator=(const UnqPtr<T>& other) = delete;

    UnqPtr(UnqPtr<T>&& other) noexcept {
        ptr = other.ptr;
        other.ptr = nullptr;
    }

    UnqPtr& operator=(UnqPtr<T>&& other) noexcept {
        if (this != &other){
            delete ptr;
            ptr = other.ptr;
            other.ptr = nullptr;
        }

        return *this;
    }

    T* get() const {
        return this->ptr;
    }

    T& operator*() const {
        return *ptr;
    }

    T* operator->() const {
        return ptr;
    }

    T* release() noexcept {
        T* temp = ptr;
        ptr = nullptr;

        return temp;
    }

    void reset(T* obj = nullptr) noexcept {
        if (obj == ptr){
            return;
        }
        delete ptr;
        ptr = obj;
    }

    void swap(UnqPtr<T>& other) noexcept {
        T* temp = ptr;
        ptr = other.get();
        other.ptr = temp;
    }
};

template <typename T>
class UnqPtr<T[]> {
private:
    T* ptr;

public:
    UnqPtr(T* obj = nullptr): ptr(obj){}

    ~UnqPtr(){
        delete[] ptr;
    }

    UnqPtr(const UnqPtr<T[]>& other) = delete;

    UnqPtr& operator=(const UnqPtr<T[]>& other) = delete;

    UnqPtr(UnqPtr<T[]>&& other) noexcept {
        ptr = other.ptr;
        other.ptr = nullptr;
    }

    UnqPtr& operator=(UnqPtr<T[]>&& other) noexcept {
        if (this != &other){
            delete[] ptr;
            ptr = other.ptr;
            other.ptr = nullptr;
        }

        return *this;
    }

    T* get() const {
        return this->ptr;
    }

    T& operator*() const {
        return *ptr;
    }

    T* operator->() const noexcept {
        return ptr;
    }

    T* release() noexcept {
        T* temp = ptr;
        ptr = nullptr;

        return temp;
    }

    void reset(T* obj = nullptr) noexcept {
        if (obj == ptr){
            return;
        }
        delete[] ptr;
        ptr = obj;
    }

    void swap(UnqPtr<T[]>& other) noexcept {
        T* temp = ptr;
        ptr = other.get();
        other.ptr = temp;
    }

    T& operator[](int index) const noexcept{
        return ptr[index];
    }
};

#endif //SMARTPTR_UNQPTR_H
