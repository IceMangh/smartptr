
#ifndef SMARTPTR_STACK_H
#define SMARTPTR_STACK_H

#include "UnqPtr.h"
#include <utility>

template <typename T>
class Stack {
private:
    struct Node {
        T value;
        UnqPtr<Node> next;

        Node(const T& value) : value(value) {}
    };

    UnqPtr<Node> head;

public:
    Stack() {
        head = UnqPtr<Node>();
    }

    ~Stack() {
        while (head.get() != nullptr){
            pop();
        }
    }

    void push(const T& value){
        UnqPtr<Node> cur(new Node(value));
        cur->next = std::move(head);
        head = std::move(cur);
    }

    void pop(){
        UnqPtr<Node> cur = std::move(head);
        head = std::move(cur->next);
    }

    T& top(){
        return head->value;
    }
};

#endif //SMARTPTR_STACK_H
