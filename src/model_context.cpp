// Copyright (c) 2025 SolverX Corporation
// This file is part of MIE OpenCascade WebAssembly Bindings.
//
// This library is free software; you can redistribute it and/or modify it under
// the terms of the GNU Lesser General Public License version 2.1 as published
// by the Free Software Foundation.

#include "model_context.hpp"

#include <emscripten/bind.h>

#include <utility>

#include "model_triangulation_impl.hpp"

// TriGeometry methods

const std::string& TriGeometry::getName() const {
    return name;
}

Float32Array TriGeometry::getPositions() const {
    emscripten::memory_view view(positions.size(), reinterpret_cast<const float*>(positions.data()));
    return Float32Array(emscripten::val(view));
}

Float32Array TriGeometry::getNormals() const {
    emscripten::memory_view view(normals.size(), reinterpret_cast<const float*>(normals.data()));
    return Float32Array(emscripten::val(view));
}

Float32Array TriGeometry::getUVs() const {
    emscripten::memory_view view(uvs.size(), reinterpret_cast<const float*>(uvs.data()));
    return Float32Array(emscripten::val(view));
}

Uint32Array TriGeometry::getIndices() const {
    emscripten::memory_view view(indices.size(), reinterpret_cast<const uint32_t*>(indices.data()));
    return Uint32Array(emscripten::val(view));
}

Uint32Array TriGeometry::getSubmeshIndices() const {
    emscripten::memory_view view(submeshIndices.size(), reinterpret_cast<const uint32_t*>(submeshIndices.data()));
    return Uint32Array(emscripten::val(view));
}

// LineGeometry methods

const std::string& LineGeometry::getName() const {
    return name;
}

Float32Array LineGeometry::getPositions() const {
    emscripten::memory_view view(positions.size(), reinterpret_cast<const float*>(positions.data()));
    return Float32Array(emscripten::val(view));
}

Uint32Array LineGeometry::getIndices() const {
    emscripten::memory_view view(indices.size(), reinterpret_cast<const uint32_t*>(indices.data()));
    return Uint32Array(emscripten::val(view));
}

Uint32Array LineGeometry::getSubmeshIndices() const {
    emscripten::memory_view view(submeshIndices.size(), reinterpret_cast<const uint32_t*>(submeshIndices.data()));
    return Uint32Array(emscripten::val(view));
}

// Material methods

Float32Array Material::getColor() const {
    emscripten::memory_view view(3, reinterpret_cast<const float*>(color.data()));
    return Float32Array(emscripten::val(view));
}

// Mesh methods

const std::string& Mesh::getName() const {
    return name;
}

Float32Array Mesh::getTransform() const {
    std::array<float, 16> matrixArray;
    for (int row = 1; row <= 4; ++row) {
        for (int col = 1; col <= 4; ++col) {
            matrixArray[(row - 1) * 4 + (col - 1)] = static_cast<float>(transform.Value(row, col));
        }
    }
    emscripten::memory_view view(16, reinterpret_cast<const float*>(matrixArray.data()));
    return Float32Array(emscripten::val(view));
}

MeshPrimitiveType Mesh::getPrimitiveType() const {
    return primitiveType;
}

size_t Mesh::getPrimitiveIndex() const {
    return primitiveIndex;
}

size_t Mesh::getMaterialIndex() const {
    return materialIndex;
}

// TriangulatedModel methods

const std::string& TriangulatedModel::getName() const {
    return name;
}

TriGeometryArray TriangulatedModel::getTris() const {
    emscripten::memory_view view(tris.size(), reinterpret_cast<const TriGeometry*>(tris.data()));
    return TriGeometryArray(emscripten::val(view));
}

LineGeometryArray TriangulatedModel::getLines() const {
    emscripten::memory_view view(lines.size(), reinterpret_cast<const LineGeometry*>(lines.data()));
    return LineGeometryArray(emscripten::val(view));
}

MaterialArray TriangulatedModel::getMaterials() const {
    emscripten::memory_view view(materials.size(), reinterpret_cast<const Material*>(materials.data()));
    return MaterialArray(emscripten::val(view));
}

MeshArray TriangulatedModel::getMeshes() const {
    emscripten::memory_view view(meshes.size(), reinterpret_cast<const Mesh*>(meshes.data()));
    return MeshArray(emscripten::val(view));
}

// ModelContext methods

void ModelContext::computeTriangulation() {
    if (triangulatedModel.has_value()) {
        return;
    }

    TCollection_ExtendedString docName = doc->GetName();
    Standard_Integer length = docName.LengthOfCString();
    Standard_Character* buffer = new Standard_Character[length + 1];
    docName.ToUTF8CString(buffer);
    std::string docNameStr(buffer, length);
    delete[] buffer;

    triangulatedModel = ModelTriangulationImpl::computeTriangulation(std::move(docNameStr), shapeTool, colorTool);
}

EMSCRIPTEN_BINDINGS(model_context_module) {
    emscripten::register_type<TriGeometryArray>("Array<TriGeometry>");
    emscripten::register_type<LineGeometryArray>("Array<LineGeometry>");
    emscripten::register_type<MaterialArray>("Array<Material>");
    emscripten::register_type<MeshArray>("Array<Mesh>");

    emscripten::class_<TriGeometry>("TriGeometry")
        .function("getName", &TriGeometry::getName)
        .function("getPositions", &TriGeometry::getPositions)
        .function("getNormals", &TriGeometry::getNormals)
        .function("getUVs", &TriGeometry::getUVs)
        .function("getIndices", &TriGeometry::getIndices)
        .function("getSubmeshIndices", &TriGeometry::getSubmeshIndices);

    emscripten::class_<LineGeometry>("LineGeometry")
        .function("getName", &LineGeometry::getName)
        .function("getPositions", &LineGeometry::getPositions)
        .function("getIndices", &LineGeometry::getIndices)
        .function("getSubmeshIndices", &LineGeometry::getSubmeshIndices);

    emscripten::class_<Material>("Material")
        .function("getColor", &Material::getColor);

    emscripten::enum_<MeshPrimitiveType>("MeshPrimitiveType")
        .value("Shell", MeshPrimitiveType::Shell)
        .value("Solid", MeshPrimitiveType::Solid)
        .value("Edge", MeshPrimitiveType::Edge)
        .value("Compound", MeshPrimitiveType::Compound)
        .value("Compsolid", MeshPrimitiveType::Compsolid);

    emscripten::class_<Mesh>("Mesh")
        .function("getName", &Mesh::getName)
        .function("getTransform", &Mesh::getTransform)
        .function("getPrimitiveType", &Mesh::getPrimitiveType)
        .function("getPrimitiveIndex", &Mesh::getPrimitiveIndex)
        .function("getMaterialIndex", &Mesh::getMaterialIndex);

    emscripten::class_<TriangulatedModel>("TriangulatedModel")
        .function("getName", &TriangulatedModel::getName)
        .function("getTris", &TriangulatedModel::getTris)
        .function("getLines", &TriangulatedModel::getLines)
        .function("getMaterials", &TriangulatedModel::getMaterials)
        .function("getMeshes", &TriangulatedModel::getMeshes);

    emscripten::register_optional<TriangulatedModel>();

    emscripten::class_<ModelContext>("ModelContext")
        .function("computeTriangulation", &ModelContext::computeTriangulation)
        .function("getTriangulatedModel", &ModelContext::getTriangulatedModel);

    emscripten::register_optional<ModelContext>();
}
