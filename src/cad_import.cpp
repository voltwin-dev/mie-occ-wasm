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
#include <istream>
#include <streambuf>
#include <optional>
#include <vector>
#ifdef __EMSCRIPTEN_PTHREADS__
#include <thread>
#endif
#include <memory>

#include <BinXCAFDrivers.hxx>
#include <STEPCAFControl_Reader.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <PCDM_ReaderStatus.hxx>
#include <TDocStd_Application.hxx>
#include <TDocStd_Document.hxx>

class ByteStreamBuf : public std::streambuf {
private:
    char* begin;
    char* end;

public:
    ByteStreamBuf(const uint8_t* data, std::size_t size)
        : begin(const_cast<char*>(reinterpret_cast<const char*>(data)))
        , end(begin + size)
    {
        setg(begin, begin, end);
    }

protected:
    pos_type seekoff(
        off_type offset,
        std::ios_base::seekdir direction,
        std::ios_base::openmode mode
    ) override {
        if ((mode & std::ios_base::in) == 0) {
            return pos_type(off_type(-1));
        }

        off_type baseOffset;
        switch (direction) {
        case std::ios_base::beg:
            baseOffset = 0;
            break;
        case std::ios_base::cur:
            baseOffset = gptr() - begin;
            break;
        case std::ios_base::end:
            baseOffset = end - begin;
            break;
        default:
            return pos_type(off_type(-1));
        }

        const off_type size = end - begin;
        if (offset < -baseOffset || offset > size - baseOffset) {
            return pos_type(off_type(-1));
        }

        char* position = begin + baseOffset + offset;
        setg(begin, position, end);
        return pos_type(position - begin);
    }

    pos_type seekpos(pos_type position, std::ios_base::openmode mode) override {
        if ((mode & std::ios_base::in) == 0) {
            return pos_type(off_type(-1));
        }

        const off_type offset = off_type(position);
        if (offset < 0 || offset > end - begin) {
            return pos_type(off_type(-1));
        }

        setg(begin, begin + offset, end);
        return position;
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

    static std::optional<ModelContext> fromXbfInternal(std::vector<uint8_t> data) {
        if (data.empty()) {
            return std::nullopt;
        }

        ByteStreamBuf byteStreamBuf(data.data(), data.size());
        std::istream stream(&byteStreamBuf);

        Handle(TDocStd_Application) app = new TDocStd_Application();
        BinXCAFDrivers::DefineFormat(app);

        Handle(TDocStd_Document) doc;
        try {
            OCC_CATCH_SIGNALS
            const PCDM_ReaderStatus status = app->Open(stream, doc);
            if (status != PCDM_RS_OK || doc.IsNull()) {
                return std::nullopt;
            }
        } catch (const Standard_Failure& e) {
            std::cerr << "XBF import error: " << e.GetMessageString() << std::endl;
            return std::nullopt;
        }

        return ModelContext(doc);
    }

public:
    static std::optional<ModelContext> fromStep(const Uint8Array& buffer) {
        std::vector<uint8_t> data = emscripten::convertJSArrayToNumberVector<uint8_t>(buffer);
        return fromStepInternal(std::move(data));
    }

    static std::optional<ModelContext> fromXbf(const Uint8Array& buffer) {
        std::vector<uint8_t> data = emscripten::convertJSArrayToNumberVector<uint8_t>(buffer);
        return fromXbfInternal(std::move(data));
    }

#ifdef __EMSCRIPTEN_PTHREADS__
    static void fromStepAsync(const Uint8Array& buffer, CadImportAsyncTask& task) {
        std::vector<uint8_t> data = emscripten::convertJSArrayToNumberVector<uint8_t>(buffer);
        std::thread([data = std::move(data), &task]() {
            task.setValue(fromStepInternal(std::move(data)));
        }).detach();
    }

    static void fromXbfAsync(const Uint8Array& buffer, CadImportAsyncTask& task) {
        std::vector<uint8_t> data = emscripten::convertJSArrayToNumberVector<uint8_t>(buffer);
        std::thread([data = std::move(data), &task]() {
            task.setValue(fromXbfInternal(std::move(data)));
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
        .class_function("fromXbf", &CadImport::fromXbf, emscripten::return_value_policy::take_ownership())
#ifdef __EMSCRIPTEN_PTHREADS__
        .class_function("fromStepAsync", &CadImport::fromStepAsync)
        .class_function("fromXbfAsync", &CadImport::fromXbfAsync)
#endif
        ;
}
