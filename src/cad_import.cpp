// Copyright (c) 2025 SolverX Corporation
// This file is part of MIE OpenCascade WebAssembly Bindings.
//
// This library is free software; you can redistribute it and/or modify it under
// the terms of the GNU Lesser General Public License version 2.1 as published
// by the Free Software Foundation.

#include "model_context.hpp"

#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <cstdint>
#include <vector>
#include <streambuf>
#include <istream>
#include <optional>

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

class CadImport {
public:
    static std::optional<ModelContext> fromStep(const Uint8Array& buffer) {
        std::vector<uint8_t> data = emscripten::convertJSArrayToNumberVector<uint8_t>(buffer);
        
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
};

EMSCRIPTEN_BINDINGS(model_context) {
    emscripten::class_<CadImport>("CadImport")
        .class_function("fromStep", &CadImport::fromStep, emscripten::return_value_policy::take_ownership());
}
