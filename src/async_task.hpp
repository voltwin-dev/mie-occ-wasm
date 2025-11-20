// Copyright (c) 2025 SolverX Corporation
// This file is part of MIE OpenCascade WebAssembly Bindings.
//
// This library is free software; you can redistribute it and/or modify it under
// the terms of the GNU Lesser General Public License version 2.1 as published
// by the Free Software Foundation.

#pragma once

#include "model_context.hpp"

#include <mutex>

#ifdef __EMSCRIPTEN_PTHREADS__
template<typename T>
class AsyncTask {
public:
    using valueType = T;

private:
    std::mutex mutex;
    bool completed;
    std::optional<T> value;

public:
    AsyncTask()
        : completed(false), value(std::nullopt)
    { }

    bool isCompleted() {
        std::lock_guard<std::mutex> lock(mutex);
        return completed;
    }

    std::optional<T> takeValue() {
        std::lock_guard<std::mutex> lock(mutex);
        return std::move(value);
    }

    void setValue(std::optional<T>&& newValue) {
        std::lock_guard<std::mutex> lock(mutex);
        value = std::move(newValue);
        completed = true;
    }
};

#define CREATE_EMSCRIPTEN_ASYNC_TASK_BINDINGS(TaskType) \
    emscripten::class_<TaskType>(#TaskType) \
        .constructor<>() \
        .function("isCompleted", &TaskType::isCompleted) \
        .function("takeValue", &TaskType::takeValue, emscripten::return_value_policy::take_ownership());\
    \
    emscripten::register_optional<typename TaskType::valueType>();
#endif
