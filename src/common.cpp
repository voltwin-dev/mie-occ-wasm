#include <emscripten/bind.h>
#include <emscripten/val.h>

class MyClass {
public:
    float lerp(float a, float b, float t) {
        return (1 - t) * a + t * b;
    }
};

EMSCRIPTEN_BINDINGS(my_module) {
    emscripten::class_<MyClass>("MyClass")
        .function("lerp", &MyClass::lerp);
}
