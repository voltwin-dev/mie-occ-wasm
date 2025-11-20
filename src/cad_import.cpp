// Copyright (c) 2025 SolverX Corporation
// This file is part of MIE OpenCascade WebAssembly Bindings.
//
// This library is free software; you can redistribute it and/or modify it under
// the terms of the GNU Lesser General Public License version 2.1 as published
// by the Free Software Foundation.

#include "model_context.hpp"
#ifdef __EMSCRIPTEN_PTHREADS__
#include "async_task.hpp"
#endif

#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <cstdint>
#include <vector>
#include <streambuf>
#include <istream>
#include <optional>
#ifdef __EMSCRIPTEN_PTHREADS__
#include <thread>
#endif
#include <memory>

#include <STEPCAFControl_Reader.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <TDocStd_Document.hxx>

class ByteStreamBuf : public std::streambuf {
public:
    ByteStreamBuf(const uint8_t* data, std::size_t size) {
        char* p = const_cast<char*>(reinterpret_cast<const char*>(data));
        setg(p, p, p + size);
    }
};

#ifdef __EMSCRIPTEN_PTHREADS__
using CadImportAsyncTask = AsyncTask<ModelContext>;
#endif

class CadImport {
private:
    static std::optional<ModelContext> fromStepInternal(std::vector<uint8_t> data) {
        STEPCAFControl_Reader stepCafReader;
        stepCafReader.SetProductMetaMode(Standard_True);

        ByteStreamBuf byteStreamBuf(data.data(), data.size());
        std::istream stream(&byteStreamBuf);

        Handle(TDocStd_Document) doc = new TDocStd_Document("BinXCAF");
        try {
            OCC_CATCH_SIGNALS
            IFSelect_ReturnStatus status = stepCafReader.ReadStream("stp", stream);
            if (status != IFSelect_RetDone) {
                return std::nullopt;
            }
            
            if (!stepCafReader.Transfer(doc)) {
                return std::nullopt;
            }
        } catch (const Standard_Failure& e) {
            std::cerr << "STEP import error: " << e.GetMessageString() << std::endl;
            return std::nullopt;
        }

        return ModelContext(doc);
    }

public:
    static std::optional<ModelContext> fromStep(const Uint8Array& buffer) {
        std::vector<uint8_t> data = emscripten::convertJSArrayToNumberVector<uint8_t>(buffer);
        return fromStepInternal(std::move(data));
    }

#ifdef __EMSCRIPTEN_PTHREADS__
    static void fromStepAsync(const Uint8Array& buffer, CadImportAsyncTask& task) {
        std::vector<uint8_t> data = emscripten::convertJSArrayToNumberVector<uint8_t>(buffer);
        std::thread([data = std::move(data), &task]() {
            task.setValue(fromStepInternal(std::move(data)));
        }).detach();
    }
#endif
};

EMSCRIPTEN_BINDINGS(model_context) {
#ifdef __EMSCRIPTEN_PTHREADS__
    CREATE_EMSCRIPTEN_ASYNC_TASK_BINDINGS(CadImportAsyncTask)
#endif

    emscripten::class_<CadImport>("CadImport")
        .class_function("fromStep", &CadImport::fromStep, emscripten::return_value_policy::take_ownership())
#ifdef __EMSCRIPTEN_PTHREADS__
        .class_function("fromStepAsync", &CadImport::fromStepAsync)
#endif
        ;
}
