// Copyright (c) 2025 SolverX Corporation
// This file is part of MIE OpenCascade WebAssembly Bindings.
//
// This library is free software; you can redistribute it and/or modify it under
// the terms of the GNU Lesser General Public License version 2.1 as published
// by the Free Software Foundation.

#include "model_context.hpp"

#include <emscripten/bind.h>

// FaceGeometry methods

const std::string& FaceGeometry::getName() const {
    return name;
}

Float32Array FaceGeometry::getPositions() const {
    emscripten::memory_view view(positions.size(), reinterpret_cast<const float*>(positions.data()));
    return Float32Array(emscripten::val(view));
}

Float32Array FaceGeometry::getNormals() const {
    emscripten::memory_view view(normals.size(), reinterpret_cast<const float*>(normals.data()));
    return Float32Array(emscripten::val(view));
}

Float32Array FaceGeometry::getUVs() const {
    emscripten::memory_view view(uvs.size(), reinterpret_cast<const float*>(uvs.data()));
    return Float32Array(emscripten::val(view));
}

Uint32Array FaceGeometry::getIndices() const {
    emscripten::memory_view view(indices.size(), reinterpret_cast<const uint32_t*>(indices.data()));
    return Uint32Array(emscripten::val(view));
}

Uint32Array FaceGeometry::getSubmeshIndices() const {
    emscripten::memory_view view(submeshIndices.size(), reinterpret_cast<const uint32_t*>(submeshIndices.data()));
    return Uint32Array(emscripten::val(view));
}

// EdgeGeometry methods

const std::string& EdgeGeometry::getName() const {
    return name;
}

Float32Array EdgeGeometry::getPositions() const {
    emscripten::memory_view view(positions.size(), reinterpret_cast<const float*>(positions.data()));
    return Float32Array(emscripten::val(view));
}

Uint32Array EdgeGeometry::getIndices() const {
    emscripten::memory_view view(indices.size(), reinterpret_cast<const uint32_t*>(indices.data()));
    return Uint32Array(emscripten::val(view));
}

Uint32Array EdgeGeometry::getSubmeshIndices() const {
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

FaceGeometryArray TriangulatedModel::getFaces() const {
    emscripten::memory_view view(faces.size(), reinterpret_cast<const FaceGeometry*>(faces.data()));
    return FaceGeometryArray(emscripten::val(view));
}

EdgeGeometryArray TriangulatedModel::getEdges() const {
    emscripten::memory_view view(edges.size(), reinterpret_cast<const EdgeGeometry*>(edges.data()));
    return EdgeGeometryArray(emscripten::val(view));
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
}

EMSCRIPTEN_BINDINGS(model_context_module) {
    emscripten::register_type<FaceGeometryArray>("Array<FaceGeometry>");
    emscripten::register_type<EdgeGeometryArray>("Array<EdgeGeometry>");
    emscripten::register_type<MaterialArray>("Array<Material>");
    emscripten::register_type<MeshArray>("Array<Mesh>");

    emscripten::class_<FaceGeometry>("FaceGeometry")
        .function("getName", &FaceGeometry::getName)
        .function("getPositions", &FaceGeometry::getPositions)
        .function("getNormals", &FaceGeometry::getNormals)
        .function("getUVs", &FaceGeometry::getUVs)
        .function("getIndices", &FaceGeometry::getIndices)
        .function("getSubmeshIndices", &FaceGeometry::getSubmeshIndices);

    emscripten::class_<EdgeGeometry>("EdgeGeometry")
        .function("getName", &EdgeGeometry::getName)
        .function("getPositions", &EdgeGeometry::getPositions)
        .function("getIndices", &EdgeGeometry::getIndices)
        .function("getSubmeshIndices", &EdgeGeometry::getSubmeshIndices);

    emscripten::class_<Material>("Material")
        .function("getColor", &Material::getColor);

    emscripten::enum_<MeshPrimitiveType>("MeshPrimitiveType")
        .value("Face", MeshPrimitiveType::Face)
        .value("Edge", MeshPrimitiveType::Edge);

    emscripten::class_<Mesh>("Mesh")
        .function("getName", &Mesh::getName)
        .function("getTransform", &Mesh::getTransform)
        .function("getPrimitiveType", &Mesh::getPrimitiveType)
        .function("getPrimitiveIndex", &Mesh::getPrimitiveIndex)
        .function("getMaterialIndex", &Mesh::getMaterialIndex);

    emscripten::class_<TriangulatedModel>("TriangulatedModel")
        .function("getName", &TriangulatedModel::getName)
        .function("getFaces", &TriangulatedModel::getFaces)
        .function("getEdges", &TriangulatedModel::getEdges)
        .function("getMaterials", &TriangulatedModel::getMaterials)
        .function("getMeshes", &TriangulatedModel::getMeshes);

    emscripten::register_optional<TriangulatedModel>();

    emscripten::class_<ModelContext>("ModelContext")
        .function("computeTriangulation", &ModelContext::computeTriangulation)
        .function("getTriangulatedModel", &ModelContext::getTriangulatedModel);

    emscripten::register_optional<ModelContext>();
}
