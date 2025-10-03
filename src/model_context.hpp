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

EMSCRIPTEN_DECLARE_VAL_TYPE(TriGeometryArray);
EMSCRIPTEN_DECLARE_VAL_TYPE(LineGeometryArray);
EMSCRIPTEN_DECLARE_VAL_TYPE(MaterialArray);
EMSCRIPTEN_DECLARE_VAL_TYPE(MeshArray);

class TriGeometry {
private:
    std::string name;
    std::vector<float> positions;
    std::vector<float> normals;
    std::vector<float> uvs;
    std::vector<uint32_t> indices; // triangle indices
    std::vector<uint32_t> submeshIndices; // verticesStart, verticesCount, indicesStart, indicesCount

public:
    TriGeometry(
        std::string name,
        std::vector<float> positions,
        std::vector<float> normals,
        std::vector<float> uvs,
        std::vector<uint32_t> indices,
        std::vector<uint32_t> submeshIndices
    )
        : name(std::move(name))
        , positions(std::move(positions))
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

    const std::string& getName() const;
    Float32Array getPositions() const;
    Float32Array getNormals() const;
    Float32Array getUVs() const;
    Uint32Array getIndices() const;
    Uint32Array getSubmeshIndices() const;
};

class LineGeometry {
private:
    std::string name;
    std::vector<float> positions;
    std::vector<uint32_t> indices; // line segments
    std::vector<uint32_t> submeshIndices; // verticesStart, verticesCount, indicesStart, indicesCount

public:
    LineGeometry(
        std::string name,
        std::vector<float> positions,
        std::vector<uint32_t> indices,
        std::vector<uint32_t> submeshIndices
    )
        : name(std::move(name))
        , positions(std::move(positions))
        , indices(std::move(indices))
        , submeshIndices(std::move(submeshIndices))
    {
        positions.shrink_to_fit();
        indices.shrink_to_fit();
        submeshIndices.shrink_to_fit();
    }

    const std::string& getName() const;
    Float32Array getPositions() const;
    Uint32Array getIndices() const;
    Uint32Array getSubmeshIndices() const;
};

class Material {
private:
    std::array<float, 3> color; // RGB

public:
    Float32Array getColor() const;
};

enum class MeshPrimitiveType {
    Shell, // represented as tri
    Solid, // represented as tri
    Edge,  // represented as line
    Compound, // no geometry, only children
    Compsolid // no geometry, only children
};

class Mesh {
private:
    std::string name;
    gp_Trsf transform;
    MeshPrimitiveType primitiveType;
    size_t primitiveIndex;
    size_t materialIndex;
    size_t parentMeshIndex;

public:
    const std::string& getName() const;
    Float32Array getTransform() const;
    MeshPrimitiveType getPrimitiveType() const;
    size_t getPrimitiveIndex() const;
    size_t getMaterialIndex() const;
};

class TriangulatedModel {
private:
    std::string name;
    std::vector<TriGeometry> tris;
    std::vector<LineGeometry> lines;
    std::vector<Material> materials;
    std::vector<Mesh> meshes;
    
public:
    TriangulatedModel(
        std::string name, 
        std::vector<TriGeometry> tris,
        std::vector<LineGeometry> lines,
        std::vector<Material> materials,
        std::vector<Mesh> meshes
    )
        : name(std::move(name))
        , tris(std::move(tris))
        , lines(std::move(lines))
        , materials(std::move(materials))
        , meshes(std::move(meshes))
    {
        tris.shrink_to_fit();
        lines.shrink_to_fit();
        materials.shrink_to_fit();
        meshes.shrink_to_fit();
    }

    const std::string& getName() const;
    TriGeometryArray getTris() const;
    LineGeometryArray getLines() const;
    MaterialArray getMaterials() const;
    MeshArray getMeshes() const;
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

    const std::optional<TriangulatedModel>& getTriangulatedModel() const {
        return triangulatedModel;
    }
};
