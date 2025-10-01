// Copyright (c) 2025 SolverX Corporation
// This file is part of MIE OpenCascade WebAssembly Bindings.
//
// This library is free software; you can redistribute it and/or modify it under
// the terms of the GNU Lesser General Public License version 2.1 as published
// by the Free Software Foundation.

#include "common.hpp"

#include <emscripten/bind.h>

EMSCRIPTEN_BINDINGS(common_module) {
    emscripten::register_type<Uint8Array>("Uint8Array");
    emscripten::register_type<Uint32Array>("Uint32Array");
    emscripten::register_type<Float32Array>("Float32Array");
}
