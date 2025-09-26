// Copyright (c) 2025 SolverX Corporation
// This file is part of MIE OpenCascade WebAssembly Bindings.
//
// This library is free software; you can redistribute it and/or modify it under
// the terms of the GNU Lesser General Public License version 2.1 as published
// by the Free Software Foundation.

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
