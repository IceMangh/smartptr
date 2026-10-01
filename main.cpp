#include <iostream>
#include "UnqPtr.h"
#include <cassert>
#include <utility>

// TIP To <b>Run</b> code, press <shortcut actionId="Run"/> or click the <icon src="AllIcons.Actions.Execute"/> icon in the gutter.
int main() {
    UnqPtr<int[]> a(new int[3]{});
    a[0] = 10;
    a[1] = 20;
    a[2] = 30;

//    UnqPtr<int> c = a.get(0);

    std::cout << *(a.get() +1) <<std::endl;

    UnqPtr<int[]> b = std::move(a);

    std::cout << a[1];








    /*
     * void f(UnqPtr b){
     *      ...
     * }
     *
     * unqptr a = ...;
     * f(a) , f(std::move(a))
     *
     *
     *
     * struct Empl {
     *      ShrdPtr<Empl> collegue;
     * }
     *
     * ShrdPtr<Empl> a = ..;
     * ShrdPtr<Empl> b = ..;
     * a->collegue = b;
     * b->collegue = a;
     *
     *
     *
     *
     *
     *
     *
     *
     *
     * */

    return 0;// TIP See CLion help at <a href="https://www.jetbrains.com/help/clion/">jetbrains.com/help/clion/</a>. Also, you can try interactive lessons for CLion by selecting 'Help | Learn IDE Features' from the main menu.
}