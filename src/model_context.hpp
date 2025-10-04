// Copyright (c) 2025 SolverX Corporation
// This file is part of MIE OpenCascade WebAssembly Bindings.
//
// This library is free software; you can redistribute it and/or modify it under
// the terms of the GNU Lesser General Public License version 2.1 as published
// by the Free Software Foundation.

#pragma once

#include <emscripten/val.h>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <TDocStd_Document.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <gp_Trsf.hxx>

#include "common.hpp"

class TriGeometry {
private:
    std::vector<float> positions;
    std::vector<float> normals;
    std::vector<float> uvs;
    std::vector<uint32_t> indices; // triangle indices
    std::vector<uint32_t> submeshIndices; // verticesStart, verticesCount, indicesStart, indicesCount

public:
    TriGeometry() = default;
    TriGeometry(
        std::vector<float> positions,
        std::vector<float> normals,
        std::vector<float> uvs,
        std::vector<uint32_t> indices,
        std::vector<uint32_t> submeshIndices
    )
        : positions(std::move(positions))
        , normals(std::move(normals))
        , uvs(std::move(uvs))
        , indices(std::move(indices))
        , submeshIndices(std::move(submeshIndices))
    {
        positions.shrink_to_fit();
        normals.shrink_to_fit();
        uvs.shrink_to_fit();
        indices.shrink_to_fit();
        submeshIndices.shrink_to_fit();
    }

    Float32Array getPositions() const;
    Float32Array getNormals() const;
    Float32Array getUVs() const;
    Uint32Array getIndices() const;
    Uint32Array getSubmeshIndices() const;
};

class LineGeometry {
private:
    std::vector<float> positions;
    std::vector<uint32_t> submeshIndices; // verticesStart, verticesCount

public:
    LineGeometry() = default;
    LineGeometry(
        std::vector<float> positions,
        std::vector<uint32_t> submeshIndices
    )
        : positions(std::move(positions))
        , submeshIndices(std::move(submeshIndices))
    {
        positions.shrink_to_fit();
        submeshIndices.shrink_to_fit();
    }

    Float32Array getPositions() const;
    Uint32Array getSubmeshIndices() const;
};

class Material {
private:
    std::array<float, 3> color; // RGB

public:
    Material() = default;
    Material(std::array<float, 3> color)
        : color(color)
    {
    }
    Float32Array getColor() const;
};

enum class MeshPrimitiveType {
    Shell, // represented as tri and line
    Solid, // represented as tri and line
    Edge,  // represented as line
    Compound, // no geometry, only children
    Compsolid, // no geometry, only children
    Unknown // no geometry, only children
};

class Mesh {
private:
    std::string name;
    std::array<float, 16> transform;
    MeshPrimitiveType primitiveType;
    int triGeometryIndex;
    int lineGeometryIndex;
    int materialIndex;
    int parentMeshIndex;

public:
    Mesh(
        std::string name,
        const std::array<float, 16>& transform,
        MeshPrimitiveType primitiveType,
        int triGeometryIndex,
        int lineGeometryIndex,
        int materialIndex,
        int parentMeshIndex
    )
        : name(std::move(name))
        , transform(transform)
        , primitiveType(primitiveType)
        , triGeometryIndex(triGeometryIndex)
        , lineGeometryIndex(lineGeometryIndex)
        , materialIndex(materialIndex)
        , parentMeshIndex(parentMeshIndex)
    {
    }
    const std::string& getName() const;
    Float32Array getTransform() const;
    MeshPrimitiveType getPrimitiveType() const;
    int getTriGeometryIndex() const;
    int getLineGeometryIndex() const;
    int getMaterialIndex() const;
    int getParentMeshIndex() const;
};

class TriangulatedModel {
private:
    std::vector<TriGeometry> tris;
    std::vector<LineGeometry> lines;
    std::vector<Material> materials;
    std::vector<Mesh> meshes;
    
public:
    TriangulatedModel(
        std::vector<TriGeometry> tris,
        std::vector<LineGeometry> lines,
        std::vector<Material> materials,
        std::vector<Mesh> meshes
    )
        : tris(std::move(tris))
        , lines(std::move(lines))
        , materials(std::move(materials))
        , meshes(std::move(meshes))
    {
        tris.shrink_to_fit();
        lines.shrink_to_fit();
        materials.shrink_to_fit();
        meshes.shrink_to_fit();
    }

    size_t getTriCount() const;
    TriGeometry& getTri(size_t index);
    size_t getLineCount() const;
    LineGeometry& getLine(size_t index);
    size_t getMaterialCount() const;
    Material& getMaterial(size_t index);
    size_t getMeshCount() const;
    Mesh& getMesh(size_t index);
};

class ModelContext {
private:
    Handle(TDocStd_Document) doc;
    Handle(XCAFDoc_ShapeTool) shapeTool;
    Handle(XCAFDoc_ColorTool) colorTool;

    std::optional<TriangulatedModel> triangulatedModel;

public:
    ModelContext(Handle(TDocStd_Document) document)
        : doc(document)
        , shapeTool(XCAFDoc_DocumentTool::ShapeTool(doc->Main()))
        , colorTool(XCAFDoc_DocumentTool::ColorTool(doc->Main()))
    {
    }

    void computeTriangulation();
    std::optional<TriangulatedModel>& getTriangulatedModel();
};
