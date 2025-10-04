// Copyright (c) 2025 SolverX Corporation
// This file is part of MIE OpenCascade WebAssembly Bindings.
//
// This library is free software; you can redistribute it and/or modify it under
// the terms of the GNU Lesser General Public License version 2.1 as published
// by the Free Software Foundation.

#include "model_triangulation_impl.hpp"

#include <unordered_map>
#include <utility>
#include <vector>

#include <Bnd_Box.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <BRepBndLib.hxx>
#include <BRepLib_ToolTriangulatedShape.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRepTools.hxx>
#include <BRep_Tool.hxx>
#include <GCPnts_TangentialDeflection.hxx>
#include <gp_Trsf.hxx>
#include <gp_Pnt.hxx>
#include <Poly_Triangulation.hxx>
#include <Prs3d.hxx>
#include <Quantity_Color.hxx>
#include <TDataStd_Name.hxx>
#include <TDF_ChildIterator.hxx>
#include <TDF_Label.hxx>
#include <TopAbs_Orientation.hxx>
#include <TopExp_Explorer.hxx>
#include <TopLoc_Location.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Shell.hxx>
#include <TopoDS_Solid.hxx>
#include <TopoDS_TShape.hxx>
#include <TopTools_IndexedMapOfShape.hxx>

class TriangulationContext {
    struct TriGeometryInfo {
        Standard_Size id;
        TriGeometry geometry;
    };
    struct LineGeometryInfo {
        Standard_Size id;
        LineGeometry geometry;
    };
    struct MaterialInfo {
        Standard_Size id;
        Material material;
    };
    struct ProcessedShapeInfo {
        Standard_Integer triGeometryIndex;
        Standard_Integer lineGeometryIndex;
    };

private:
    Handle(XCAFDoc_ShapeTool) shapeTool;
    Handle(XCAFDoc_ColorTool) colorTool;

    // output data
    std::unordered_map<TopoDS_TShape*, TriGeometryInfo> triGeometryMap;
    std::unordered_map<TopoDS_TShape*, LineGeometryInfo> lineGeometryMap;
    std::unordered_map<TDF_Label, MaterialInfo> materialMap;
    std::vector<Mesh> meshes;

    // for triangulation processing
    std::unordered_map<TopoDS_TShape*, ProcessedShapeInfo> processedShapeMap;
public:
    TriangulationContext(
        Handle(XCAFDoc_ShapeTool) shapeTool,
        Handle(XCAFDoc_ColorTool) colorTool
    )        : shapeTool(shapeTool)
        , colorTool(colorTool)
    { }

    TriangulatedModel compute() {
        // build solid and edge shape ID maps
        for (TDF_ChildIterator it(shapeTool->Label()); it.More(); it.Next()) {
            TDF_Label childLabel = it.Value();
            TopoDS_Shape shape;
            if (shapeTool->GetShape(childLabel, shape) && shapeTool->IsFree(childLabel)) {
                // free shapes (root nodes)
                resolveShapeTree(shape);
            }
        }
        
        processedShapeMap.clear();

        std::vector<TriGeometry> tris(triGeometryMap.size());
        for (const auto& [_, triInfo] : triGeometryMap) tris[triInfo.id] = std::move(triInfo.geometry);
        triGeometryMap.clear();
        std::vector<LineGeometry> lines(lineGeometryMap.size());
        for (const auto& [_, lineInfo] : lineGeometryMap) lines[lineInfo.id] = std::move(lineInfo.geometry);
        lineGeometryMap.clear();
        std::vector<Material> materials(materialMap.size());
        for (const auto& [_, matInfo] : materialMap) materials[matInfo.id] = std::move(matInfo.material);
        materialMap.clear();

        // clean up triangulation data to save memory
        for (TDF_ChildIterator it(shapeTool->Label()); it.More(); it.Next()) {
            TDF_Label childLabel = it.Value();
            TopoDS_Shape shape;
            if (shapeTool->GetShape(childLabel, shape) && shapeTool->IsFree(childLabel)) {
                BRepTools::Clean(shape, true);
            }
        }

        return TriangulatedModel(
            std::move(tris),
            std::move(lines),
            std::move(materials),
            std::move(meshes)
        );
    }

private:
    TDF_Label resolveReferredShapeLabel(const TDF_Label& label) const {
        TDF_Label resolvedLabel = label;
        while (XCAFDoc_ShapeTool::IsReference(resolvedLabel)) {
            TDF_Label refLabel;
            shapeTool->GetReferredShape(resolvedLabel, refLabel);
            resolvedLabel = refLabel;
        }
        return resolvedLabel;
    }

    bool getLabelColor(const TDF_Label& label, Quantity_Color& color) const {
        static constexpr std::array<XCAFDoc_ColorType, 3> colorTypes = { XCAFDoc_ColorSurf, XCAFDoc_ColorCurv, XCAFDoc_ColorGen };
        for (XCAFDoc_ColorType colorType : colorTypes) {
            if (colorTool->GetColor(label, colorType, color)) {
                return true;
            }
        }
        return false;
    }

    std::string getLabelName(const TDF_Label& label) const {
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

    // shape must be TopoDS_Shell or TopoDS_Solid
    ProcessedShapeInfo triangulateShape(const TopoDS_Shape& shape) {
        // check if already processed
        if (processedShapeMap.find(shape.TShape().get()) != processedShapeMap.end()) {
            return processedShapeMap[shape.TShape().get()];
        }

        Standard_Integer triGeometryIndex = -1;
        Standard_Integer lineGeometryIndex = -1;

        gp_Trsf parentTransform = shape.Location().Transformation(); // parent global transform

        // from StdPrs_ToolTriangulatedShape::GetDeflection
        constexpr Standard_Real MAXIMAL_CHORDAL_DEVIATION = 0.0001;
        constexpr Standard_Real DEVIATION_COEFFICIENT = 0.001;
        Bnd_Box boundBox;
        BRepBndLib::Add(shape, boundBox, Standard_False);
        const Standard_Real deflection = Prs3d::GetDeflection(boundBox, DEVIATION_COEFFICIENT, MAXIMAL_CHORDAL_DEVIATION);
        
        constexpr Standard_Real ANGLE_DEFLECTION = 0.2;
        BRepMesh_IncrementalMesh mesh(
            shape, // The shape to mesh
            deflection, // Linear deflection
            true,  // Relative
            ANGLE_DEFLECTION, // Angular deflection
            true   // In parallel
        );

        {
            std::vector<float> positions;
            std::vector<float> normals;
            std::vector<float> uvs;
            std::vector<uint32_t> indices;
            std::vector<uint32_t> submeshIndices;

            TopExp_Explorer faceExplorer(shape, TopAbs_FACE);
            for (; faceExplorer.More(); faceExplorer.Next()) {
                const TopoDS_Face& face = TopoDS::Face(faceExplorer.Current());
                
                TopLoc_Location location;
                Handle(Poly_Triangulation) polyTri = BRep_Tool::Triangulation(face, location);

                if (!polyTri.IsNull()) {
                    gp_Trsf childTransform = location.Transformation(); // child global transform
                    gp_Trsf relativeTransform = parentTransform.Inverted().Multiplied(childTransform); // relative transform from parent to child
                    TopAbs_Orientation faceOrientation = face.Orientation();
                    Standard_Size indexOffset = static_cast<Standard_Size>(positions.size() / 3);

                    submeshIndices.push_back(static_cast<uint32_t>(indexOffset)); // vertex start
                    submeshIndices.push_back(static_cast<uint32_t>(polyTri->NbNodes())); // vertex count
                    submeshIndices.push_back(static_cast<uint32_t>(indices.size())); // index start
                    submeshIndices.push_back(static_cast<uint32_t>(polyTri->NbTriangles() * 3)); // index count

                    for (Standard_Integer i = 1; i <= polyTri->NbNodes(); ++i) {
                        gp_Pnt pnt = polyTri->Node(i).Transformed(relativeTransform);
                        positions.push_back(static_cast<float>(pnt.X()));
                        positions.push_back(static_cast<float>(pnt.Y()));
                        positions.push_back(static_cast<float>(pnt.Z()));
                    }

                    BRepLib_ToolTriangulatedShape::ComputeNormals(face, polyTri);
                    Standard_Boolean fixNormals = (faceOrientation == TopAbs_REVERSED) ^ (relativeTransform.VectorialPart().Determinant() < 0);
                    if (fixNormals) {
                        for (Standard_Integer i = 1; i <= polyTri->NbNodes(); ++i) {
                            gp_Dir normal = polyTri->Normal(i).Reversed().Transformed(relativeTransform);
                            normals.push_back(static_cast<float>(normal.X()));
                            normals.push_back(static_cast<float>(normal.Y()));
                            normals.push_back(static_cast<float>(normal.Z()));
                        }
                    } else {
                        for (Standard_Integer i = 1; i <= polyTri->NbNodes(); ++i) {
                            gp_Dir normal = polyTri->Normal(i).Transformed(relativeTransform);
                            normals.push_back(static_cast<float>(normal.X()));
                            normals.push_back(static_cast<float>(normal.Y()));
                            normals.push_back(static_cast<float>(normal.Z()));
                        }
                    }

                    Standard_Real umin, umax, vmin, vmax;
                    BRepTools::UVBounds(face, umin, umax, vmin, vmax);
                    for (Standard_Integer i = 1; i <= polyTri->NbNodes(); ++i) {
                        gp_Pnt2d uv = polyTri->UVNode(i);
                        float u = static_cast<float>((uv.X() - umin) / (umax - umin));
                        float v = static_cast<float>((uv.Y() - vmin) / (vmax - vmin));
                        uvs.push_back(u);
                        uvs.push_back(v);
                    }

                    if (faceOrientation == TopAbs_REVERSED) {
                        // reverse triangle winding order
                        for (Standard_Integer i = 1; i <= polyTri->NbTriangles(); ++i) {
                            Standard_Integer n1, n2, n3;
                            polyTri->Triangle(i).Get(n1, n2, n3);
                            // convert to zero-based index
                            indices.push_back(static_cast<uint32_t>(n1 - 1 + indexOffset));
                            indices.push_back(static_cast<uint32_t>(n3 - 1 + indexOffset));
                            indices.push_back(static_cast<uint32_t>(n2 - 1 + indexOffset));
                        }
                    } else {
                        for (Standard_Integer i = 1; i <= polyTri->NbTriangles(); ++i) {
                            Standard_Integer n1, n2, n3;
                            polyTri->Triangle(i).Get(n1, n2, n3);
                            // convert to zero-based index
                            indices.push_back(static_cast<uint32_t>(n1 - 1 + indexOffset));
                            indices.push_back(static_cast<uint32_t>(n2 - 1 + indexOffset));
                            indices.push_back(static_cast<uint32_t>(n3 - 1 + indexOffset));
                        }
                    }
                }
            }
        
            if (!positions.empty() && !indices.empty()) {
                TriGeometryInfo newTriInfo = {
                    .id = static_cast<Standard_UInteger>(triGeometryMap.size()),
                    .geometry = TriGeometry(
                        std::move(positions),
                        std::move(normals),
                        std::move(uvs),
                        std::move(indices),
                        std::move(submeshIndices)
                    )
                };
                const auto [triIt, triInserted] = triGeometryMap.emplace(shape.TShape().get(), std::move(newTriInfo));
                triGeometryIndex = static_cast<Standard_Integer>(triIt->second.id);
            }
        }

        {
            std::vector<float> positions;
            std::vector<uint32_t> submeshIndices;

            TopExp_Explorer edgeExplorer(shape, TopAbs_EDGE);
            for (; edgeExplorer.More(); edgeExplorer.Next()) {
                TopoDS_Edge edge = TopoDS::Edge(edgeExplorer.Current());

                gp_Trsf childTransform = edge.Location().Transformation();
                gp_Trsf relativeTransform = parentTransform.Inverted().Multiplied(childTransform); // relative transform from parent to child
                edge.Location(TopLoc_Location(relativeTransform));

                BRepAdaptor_Curve curve(edge);
                GCPnts_TangentialDeflection points(curve, ANGLE_DEFLECTION, deflection);
                if (points.NbPoints() < 2) continue; // NOTE: this might be unreachable
                for (int i = 1; i < points.NbPoints(); i++) {
                    gp_Pnt pnt1 = points.Value(i);
                    gp_Pnt pnt2 = points.Value(i + 1);
                    
                    positions.push_back(static_cast<float>(pnt1.X()));
                    positions.push_back(static_cast<float>(pnt1.Y()));
                    positions.push_back(static_cast<float>(pnt1.Z()));

                    positions.push_back(static_cast<float>(pnt2.X()));
                    positions.push_back(static_cast<float>(pnt2.Y()));
                    positions.push_back(static_cast<float>(pnt2.Z()));
                }
            }

            if (!positions.empty()) {
                LineGeometryInfo newLineInfo = {
                    .id = static_cast<Standard_UInteger>(lineGeometryMap.size()),
                    .geometry = LineGeometry(
                        std::move(positions),
                        std::move(submeshIndices)
                    )
                };
                const auto [lineIt, lineInserted] = lineGeometryMap.emplace(shape.TShape().get(), std::move(newLineInfo));
                lineGeometryIndex = static_cast<Standard_Integer>(lineIt->second.id);
            }
        }
        
        ProcessedShapeInfo processedInfo = {
            .triGeometryIndex = triGeometryIndex,
            .lineGeometryIndex = lineGeometryIndex
        };
        processedShapeMap[shape.TShape().get()] = processedInfo;
        return processedInfo;
    }

    void resolveShapeTree(const TopoDS_Shape& rootShape) {
        struct StackFrame {
            TopoDS_Shape shape;
            Standard_Integer parentMeshIndex;
        };
        std::vector<StackFrame> stack;
        stack.push_back({ rootShape, -1 });

        while (!stack.empty()) {
            auto [shape, parentMeshIndex] = stack.back();
            stack.pop_back();

            Standard_Integer meshIndex = static_cast<Standard_Integer>(meshes.size());
            gp_Trsf shapeTransform = shape.Location().Transformation();
            MeshPrimitiveType primitiveType;
            switch (shape.ShapeType()) {
            case TopAbs_COMPOUND: primitiveType = MeshPrimitiveType::Compound; break;
            case TopAbs_COMPSOLID: primitiveType = MeshPrimitiveType::Compsolid; break;
            case TopAbs_SOLID: primitiveType = MeshPrimitiveType::Solid; break;
            case TopAbs_SHELL: primitiveType = MeshPrimitiveType::Shell; break;
            case TopAbs_EDGE: primitiveType = MeshPrimitiveType::Edge; break;
            default: primitiveType = MeshPrimitiveType::Unknown; break;
            }

            // resolve shape name and material index
            std::string shapeName;
            Standard_Integer materialIndex = -1;
            if (TDF_Label shapeLabel; shapeTool->Search(shape, shapeLabel)) {
                shapeName = getLabelName(shapeLabel);
                
                TDF_Label resolvedLabel = resolveReferredShapeLabel(shapeLabel);
                Quantity_Color color;
                if (getLabelColor(resolvedLabel, color)) { // NOTE: multiple resolution occuring here. can be optimized
                    MaterialInfo newMaterialInfo = {
                        .id = static_cast<Standard_UInteger>(materialMap.size()),
                        .material = Material({
                            static_cast<float>(color.Red()),
                            static_cast<float>(color.Green()),
                            static_cast<float>(color.Blue())
                        })
                    };
                    const auto [matIt, matInserted] = materialMap.emplace(resolvedLabel, newMaterialInfo);
                    materialIndex = static_cast<Standard_Integer>(matIt->second.id);
                }
            }

            // resolve primitive index
            Standard_Integer triGeometryIndex = -1;
            Standard_Integer lineGeometryIndex = -1;
            if (shape.ShapeType() == TopAbs_COMPOUND || shape.ShapeType() == TopAbs_COMPSOLID) {
                // resolve sub-shapes if compound shape
                TopoDS_Iterator it(shape);
                for (; it.More(); it.Next()) {
                    stack.push_back({ it.Value(), meshIndex });
                }
            } else if (shape.ShapeType() == TopAbs_SOLID || shape.ShapeType() == TopAbs_SHELL) {
                ProcessedShapeInfo processedInfo = triangulateShape(shape);
                triGeometryIndex = processedInfo.triGeometryIndex;
                lineGeometryIndex = processedInfo.lineGeometryIndex;
            }

            meshes.push_back(Mesh(
                std::move(shapeName),
                shapeTransform,
                primitiveType,
                triGeometryIndex,
                lineGeometryIndex,
                materialIndex,
                parentMeshIndex
            ));
        }
    }
};

TriangulatedModel ModelTriangulationImpl::computeTriangulation(
    Handle(XCAFDoc_ShapeTool)& shapeTool,
    Handle(XCAFDoc_ColorTool)& colorTool
) {
    TriangulationContext context(shapeTool, colorTool);
    return context.compute();
}
