// Copyright (c) 2025 SolverX Corporation
// This file is part of MIE OpenCascade WebAssembly Bindings.
//
// This library is free software; you can redistribute it and/or modify it under
// the terms of the GNU Lesser General Public License version 2.1 as published
// by the Free Software Foundation.

#pragma once

#include "model_triangulation_impl.hpp"

#include <unordered_map>
#include <utility>
#include <vector>

#include <TDataStd_Name.hxx>
#include <TDF_ChildIterator.hxx>
#include <TDF_Label.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Shell.hxx>
#include <TopoDS_Solid.hxx>
#include <TopoDS_TShape.hxx>

TDF_Label resolveReferredShapeLabel(
    const TDF_Label& label,
    Handle(XCAFDoc_ShapeTool)& shapeTool
) {
    TDF_Label resolvedLabel = label;
    while (XCAFDoc_ShapeTool::IsReference(resolvedLabel)) {
        TDF_Label refLabel;
        shapeTool->GetReferredShape(resolvedLabel, refLabel);
        resolvedLabel = refLabel;
    }
    return resolvedLabel;
}

bool getLabelColor(
    const TDF_Label& label,
    Handle(XCAFDoc_ShapeTool)& shapeTool,
    Handle(XCAFDoc_ColorTool)& colorTool,
    Quantity_Color& color
) {
    static constexpr std::array<XCAFDoc_ColorType, 3> colorTypes = { XCAFDoc_ColorSurf, XCAFDoc_ColorCurv, XCAFDoc_ColorGen };
    for (XCAFDoc_ColorType colorType : colorTypes) {
        if (colorTool->GetColor(label, colorType, color)) {
            return true;
        }
    }
    return false;
}

std::string getLabelName(
    const TDF_Label& label,
    Handle(XCAFDoc_ShapeTool)& shapeTool
) {
    Handle(TDataStd_Name) nameAttr;
    if (label.FindAttribute(TDataStd_Name::GetID(), nameAttr)) {
        Standard_Integer length = nameAttr->Get().LengthOfCString();
        Standard_Character* buffer = new Standard_Character[length + 1];
        nameAttr->Get().ToUTF8CString(buffer);
        std::string result(buffer, length);
        delete[] buffer;
        return result;
    }
    return std::string();
}

void resolveShapeTree(
    const TopoDS_Shape& rootShape,
    Handle(XCAFDoc_ShapeTool)& shapeTool,
    Handle(XCAFDoc_ColorTool)& colorTool,
    std::vector<Mesh>& meshes
) {
}

std::optional<TriangulatedModel> ModelTriangulationImpl::computeTriangulation(
    std::string modelName,
    Handle(XCAFDoc_ShapeTool)& shapeTool,
    Handle(XCAFDoc_ColorTool)& colorTool
) {
    struct GeometryInfo {
        Standard_UInteger id;
        std::string name = std::string();
        Standard_Boolean isReferenced = Standard_False;
    };
    std::unordered_map<TopoDS_TShape*, GeometryInfo> solidShapeIdMap;
    std::unordered_map<TopoDS_TShape*, GeometryInfo> edgeShapeIdMap;

    std::vector<Mesh> meshes;

    for (TDF_ChildIterator it(shapeTool->Label()); it.More(); it.Next()) {
        TDF_Label childLabel = it.Value();
        TopoDS_Shape shape;
        if (shapeTool->GetShape(childLabel, shape) && shapeTool->IsFree(childLabel)) {
            // free shapes (root nodes)
            
            // collect all face TShapes to build solid/shell list
            for (TopExp_Explorer exp(shape, TopAbs_SOLID); exp.More(); exp.Next()) {
                TopoDS_Solid solid = TopoDS::Solid(exp.Current());
                GeometryInfo newinfo = { .id = static_cast<Standard_UInteger>(solidShapeIdMap.size()) };
                const auto [it, inserted] = solidShapeIdMap.emplace(solid.TShape().get(), newinfo);
                if (inserted) {
                    TDF_Label resolvedLabel = resolveReferredShapeLabel(childLabel, shapeTool);
                    it->second.name = getLabelName(resolvedLabel, shapeTool);
                }   
            }
            for (TopExp_Explorer exp(shape, TopAbs_SHELL); exp.More(); exp.Next()) {
                TopoDS_Shell shell = TopoDS::Shell(exp.Current());
                GeometryInfo newinfo = { .id = static_cast<Standard_UInteger>(solidShapeIdMap.size()) };
                const auto [it, inserted] = solidShapeIdMap.emplace(shell.TShape().get(), newinfo);
                if (inserted) {
                    TDF_Label resolvedLabel = resolveReferredShapeLabel(childLabel, shapeTool);
                    it->second.name = getLabelName(resolvedLabel, shapeTool);
                }
            }

            // collect all edge TShapes to build edge list
            for (TopExp_Explorer exp(shape, TopAbs_EDGE, TopAbs_FACE); exp.More(); exp.Next()) {
                TopoDS_Edge edge = TopoDS::Edge(exp.Current());
                GeometryInfo newinfo = { .name = std::string(), .id = static_cast<Standard_UInteger>(edgeShapeIdMap.size()) };
                const auto [it, inserted] = edgeShapeIdMap.emplace(edge.TShape().get(), newinfo);
                if (inserted) {
                    TDF_Label resolvedLabel = resolveReferredShapeLabel(childLabel, shapeTool);
                    it->second.name = getLabelName(resolvedLabel, shapeTool);
                }
            }

            // traverse shape to preserve hierarchy and generate meshes
            resolveShapeTree(shape, shapeTool, colorTool, meshes);
        }
    }

    TDF_LabelSequence colorLabels;
    colorTool->GetColors(colorLabels);
}
