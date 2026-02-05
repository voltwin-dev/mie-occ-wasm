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
#ifdef __EMSCRIPTEN_PTHREADS__
#include <mutex>
#endif

#include <TDocStd_Document.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <gp_Trsf.hxx>

#include "common.hpp"
#ifdef __EMSCRIPTEN_PTHREADS__
#include "async_task.hpp"
#endif

class TriGeometry {
public:
    std::vector<float> positions;
    std::vector<float> normals;
    std::vector<float> uvs;
    std::vector<uint32_t> indices; // triangle indices
    std::vector<uint32_t> subMeshIndices; // verticesCount

public:
    TriGeometry() = default;
    TriGeometry(
        std::vector<float> positions,
        std::vector<float> normals,
        std::vector<float> uvs,
        std::vector<uint32_t> indices,
        std::vector<uint32_t> subMeshIndices
    )
        : positions(std::move(positions))
        , normals(std::move(normals))
        , uvs(std::move(uvs))
        , indices(std::move(indices))
        , subMeshIndices(std::move(subMeshIndices))
    {
        positions.shrink_to_fit();
        normals.shrink_to_fit();
        uvs.shrink_to_fit();
        indices.shrink_to_fit();
        subMeshIndices.shrink_to_fit();
    }

    Float32Array getPositions() const;
    Float32Array getNormals() const;
    Float32Array getUVs() const;
    Uint32Array getIndices() const;
    Uint32Array getSubMeshIndices() const;
};

class LineGeometry {
public:
    std::vector<float> positions;
    std::vector<uint32_t> subMeshIndices; // verticesCount

public:
    LineGeometry() = default;
    LineGeometry(
        std::vector<float> positions,
        std::vector<uint32_t> subMeshIndices
    )
        : positions(std::move(positions))
        , subMeshIndices(std::move(subMeshIndices))
    {
        positions.shrink_to_fit();
        subMeshIndices.shrink_to_fit();
    }

    Float32Array getPositions() const;
    Uint32Array getSubMeshIndices() const;
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

class PointGeometry {
public:
    std::vector<float> positions;

public:
    PointGeometry() = default;
    PointGeometry(std::vector<float> positions)
        : positions(std::move(positions))
    {
        positions.shrink_to_fit();
    }

    Float32Array getPositions() const;
};

enum class MeshShapeType {
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
    MeshShapeType shapeType;
    int triGeometryIndex;
    int lineGeometryIndex;
    int pointGeometryIndex;
    int materialIndex;
    int parentMeshIndex;

public:
    Mesh(
        std::string name,
        const std::array<float, 16>& transform,
        MeshShapeType shapeType,
        int triGeometryIndex,
        int lineGeometryIndex,
        int pointGeometryIndex,
        int materialIndex,
        int parentMeshIndex
    )
        : name(std::move(name))
        , transform(transform)
        , shapeType(shapeType)
        , triGeometryIndex(triGeometryIndex)
        , lineGeometryIndex(lineGeometryIndex)
        , pointGeometryIndex(pointGeometryIndex)
        , materialIndex(materialIndex)
        , parentMeshIndex(parentMeshIndex)
    {
    }
    const std::string& getName() const;
    Float32Array getTransform() const;
    MeshShapeType getShapeType() const;
    int getTriGeometryIndex() const;
    int getLineGeometryIndex() const;
    int getPointGeometryIndex() const;
    int getMaterialIndex() const;
    int getParentMeshIndex() const;
};

class TriangulatedModel {
private:
    std::vector<TriGeometry> tris;
    std::vector<LineGeometry> lines;
    std::vector<PointGeometry> points;
    std::vector<Material> materials;
    std::vector<Mesh> meshes;
    
public:
    TriangulatedModel(
        std::vector<TriGeometry> tris,
        std::vector<LineGeometry> lines,
        std::vector<PointGeometry> points,
        std::vector<Material> materials,
        std::vector<Mesh> meshes
    )
        : tris(std::move(tris))
        , lines(std::move(lines))
        , points(std::move(points))
        , materials(std::move(materials))
        , meshes(std::move(meshes))
    {
        tris.shrink_to_fit();
        lines.shrink_to_fit();
        points.shrink_to_fit();
        materials.shrink_to_fit();
        meshes.shrink_to_fit();
    }

    size_t getTriCount() const;
    TriGeometry& getTri(size_t index);
    size_t getLineCount() const;
    LineGeometry& getLine(size_t index);
    size_t getPointCount() const;
    PointGeometry& getPoint(size_t index);
    size_t getMaterialCount() const;
    Material& getMaterial(size_t index);
    size_t getMeshCount() const;
    Mesh& getMesh(size_t index);
};

#ifdef __EMSCRIPTEN_PTHREADS__
using TriangulationAsyncTask = AsyncTask<bool>;
#endif

class ModelContext {
private:
    Handle(TDocStd_Document) doc;
    Handle(XCAFDoc_ShapeTool) shapeTool;
    Handle(XCAFDoc_ColorTool) colorTool;

    std::optional<TriangulatedModel> triangulatedModel;
#ifdef __EMSCRIPTEN_PTHREADS__
    mutable std::mutex triangulationMutex;
#endif

public:
    ModelContext(Handle(TDocStd_Document) document)
        : doc(document)
        , shapeTool(XCAFDoc_DocumentTool::ShapeTool(doc->Main()))
        , colorTool(XCAFDoc_DocumentTool::ColorTool(doc->Main()))
    {
    }

    ModelContext(const ModelContext& other) :
        doc(other.doc),
        shapeTool(other.shapeTool),
        colorTool(other.colorTool)
    {
#ifdef __EMSCRIPTEN_PTHREADS__
        std::lock_guard<std::mutex> lock(other.triangulationMutex);
#endif
        triangulatedModel = other.triangulatedModel;
    }

    ModelContext(ModelContext&& other) noexcept :
        doc(std::move(other.doc)),
        shapeTool(std::move(other.shapeTool)),
        colorTool(std::move(other.colorTool))
    {
#ifdef __EMSCRIPTEN_PTHREADS__
        std::lock_guard<std::mutex> lock(other.triangulationMutex);
#endif
        triangulatedModel = std::move(other.triangulatedModel);
    }

    ModelContext& operator=(const ModelContext& other) {
        if (this != &other) {
            doc = other.doc;
            shapeTool = other.shapeTool;
            colorTool = other.colorTool;
#ifdef __EMSCRIPTEN_PTHREADS__
            std::lock_guard<std::mutex> lockOther(other.triangulationMutex);
            std::lock_guard<std::mutex> lockThis(triangulationMutex);
#endif
            triangulatedModel = other.triangulatedModel;
        }
        return *this;
    }

    ModelContext& operator=(ModelContext&& other) noexcept {
        if (this != &other) {
            doc = std::move(other.doc);
            shapeTool = std::move(other.shapeTool);
            colorTool = std::move(other.colorTool);
#ifdef __EMSCRIPTEN_PTHREADS__
            std::lock_guard<std::mutex> lockOther(other.triangulationMutex);
            std::lock_guard<std::mutex> lockThis(triangulationMutex);
#endif
            triangulatedModel = std::move(other.triangulatedModel);
        }
        return *this;
    }

    void computeTriangulation();
#ifdef __EMSCRIPTEN_PTHREADS__
    void computeTriangulationAsync(TriangulationAsyncTask& task);
#endif
    std::optional<TriangulatedModel>& getTriangulatedModel();
};
