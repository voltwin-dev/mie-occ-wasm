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

Float32Array LineGeometry::getPositions() const {
    emscripten::memory_view view(positions.size(), reinterpret_cast<const float*>(positions.data()));
    return Float32Array(emscripten::val(view));
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
    emscripten::memory_view view(16, reinterpret_cast<const float*>(transform.data()));
    return Float32Array(emscripten::val(view));
}

MeshShapeType Mesh::getShapeType() const {
    return shapeType;
}

int Mesh::getTriGeometryIndex() const {
    return triGeometryIndex;
}

int Mesh::getLineGeometryIndex() const {
    return lineGeometryIndex;
}

int Mesh::getMaterialIndex() const {
    return materialIndex;
}

int Mesh::getParentMeshIndex() const {
    return parentMeshIndex;
}

// TriangulatedModel methods

size_t TriangulatedModel::getTriCount() const {
    return tris.size();
}

TriGeometry& TriangulatedModel::getTri(size_t index) {
    return tris[index];
}

size_t TriangulatedModel::getLineCount() const {
    return lines.size();
}

LineGeometry& TriangulatedModel::getLine(size_t index) {
    return lines[index];
}

size_t TriangulatedModel::getMaterialCount() const {
    return materials.size();
}

Material& TriangulatedModel::getMaterial(size_t index) {
    return materials[index];
}

size_t TriangulatedModel::getMeshCount() const {
    return meshes.size();
}

Mesh& TriangulatedModel::getMesh(size_t index) {
    return meshes[index];
}

// ModelContext methods

void ModelContext::computeTriangulation() {
    if (triangulatedModel.has_value()) {
        return;
    }

    triangulatedModel = ModelTriangulationImpl::computeTriangulation(shapeTool, colorTool);
}

std::optional<TriangulatedModel>& ModelContext::getTriangulatedModel() {
    return triangulatedModel;
}

EMSCRIPTEN_BINDINGS(model_context_module) {
    emscripten::class_<TriGeometry>("TriGeometry")
        .function("getPositions", &TriGeometry::getPositions)
        .function("getNormals", &TriGeometry::getNormals)
        .function("getUVs", &TriGeometry::getUVs)
        .function("getIndices", &TriGeometry::getIndices)
        .function("getSubmeshIndices", &TriGeometry::getSubmeshIndices);

    emscripten::class_<LineGeometry>("LineGeometry")
        .function("getPositions", &LineGeometry::getPositions)
        .function("getSubmeshIndices", &LineGeometry::getSubmeshIndices);

    emscripten::class_<Material>("Material")
        .function("getColor", &Material::getColor);

    emscripten::enum_<MeshShapeType>("MeshShapeType")
        .value("Shell", MeshShapeType::Shell)
        .value("Solid", MeshShapeType::Solid)
        .value("Edge", MeshShapeType::Edge)
        .value("Compound", MeshShapeType::Compound)
        .value("Compsolid", MeshShapeType::Compsolid)
        .value("Unknown", MeshShapeType::Unknown);

    emscripten::class_<Mesh>("Mesh")
        .function("getName", &Mesh::getName)
        .function("getTransform", &Mesh::getTransform)
        .function("getShapeType", &Mesh::getShapeType)
        .function("getTriGeometryIndex", &Mesh::getTriGeometryIndex)
        .function("getLineGeometryIndex", &Mesh::getLineGeometryIndex)
        .function("getMaterialIndex", &Mesh::getMaterialIndex)
        .function("getParentMeshIndex", &Mesh::getParentMeshIndex);

    emscripten::class_<TriangulatedModel>("TriangulatedModel")
        .function("getTriCount", &TriangulatedModel::getTriCount)
        .function("getTri", &TriangulatedModel::getTri, emscripten::return_value_policy::reference())
        .function("getLineCount", &TriangulatedModel::getLineCount)
        .function("getLine", &TriangulatedModel::getLine, emscripten::return_value_policy::reference())
        .function("getMaterialCount", &TriangulatedModel::getMaterialCount)
        .function("getMaterial", &TriangulatedModel::getMaterial, emscripten::return_value_policy::reference())
        .function("getMeshCount", &TriangulatedModel::getMeshCount)
        .function("getMesh", &TriangulatedModel::getMesh, emscripten::return_value_policy::reference());

    emscripten::register_optional<TriangulatedModel>();

    emscripten::class_<ModelContext>("ModelContext")
        .function("computeTriangulation", &ModelContext::computeTriangulation)
        .function("getTriangulatedModel", &ModelContext::getTriangulatedModel, emscripten::return_value_policy::reference());

    emscripten::register_optional<ModelContext>();
}
