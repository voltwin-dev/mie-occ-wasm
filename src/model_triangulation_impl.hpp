// Copyright (c) 2025 SolverX Corporation
// This file is part of MIE OpenCascade WebAssembly Bindings.
//
// This library is free software; you can redistribute it and/or modify it under
// the terms of the GNU Lesser General Public License version 2.1 as published
// by the Free Software Foundation.

#pragma once

#include <string>

#include <XCAFDoc_ShapeTool.hxx>
#include <XCAFDoc_ColorTool.hxx>

#include "model_context.hpp"

class ModelTriangulationImpl {
public:
    static TriangulatedModel computeTriangulation(
        std::string modelName,
        Handle(XCAFDoc_ShapeTool)& shapeTool,
        Handle(XCAFDoc_ColorTool)& colorTool
    );
};
