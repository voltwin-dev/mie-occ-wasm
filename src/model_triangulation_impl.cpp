// Copyright (c) 2025 SolverX Corporation
// This file is part of MIE OpenCascade WebAssembly Bindings.
//
// This library is free software; you can redistribute it and/or modify it under
// the terms of the GNU Lesser General Public License version 2.1 as published
// by the Free Software Foundation.

#include "model_triangulation_impl.hpp"

#include <array>
#include <cmath>
#include <unordered_map>
#include <unordered_set>
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
#include <Poly_PolygonOnTriangulation.hxx>
#include <Precision.hxx>
#include <Prs3d.hxx>
#include <Quantity_Color.hxx>
#include <TColStd_Array1OfInteger.hxx>
#include <TDataStd_Name.hxx>
#include <TDF_ChildIterator.hxx>
#include <TDF_Label.hxx>
#include <TDF_LabelSequence.hxx>
#include <TDF_Tool.hxx>
#include <TCollection_AsciiString.hxx>
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
#include <TopoDS_Vertex.hxx>
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
    struct PointGeometryInfo {
        Standard_Size id;
        PointGeometry geometry;
    };
    struct MaterialInfo {
        Standard_Size id;
        Material material;
    };
    struct ProcessedShapeInfo {
        Standard_Integer triGeometryIndex;
        Standard_Integer lineGeometryIndex;
        Standard_Integer pointGeometryIndex;
    };

private:
    Handle(XCAFDoc_ShapeTool) shapeTool;
    Handle(XCAFDoc_ColorTool) colorTool;

    // output data
    std::unordered_map<TopoDS_TShape*, TriGeometryInfo> triGeometryMap;
    std::unordered_map<TopoDS_TShape*, LineGeometryInfo> lineGeometryMap;
    std::unordered_map<TopoDS_TShape*, PointGeometryInfo> pointGeometryMap;
    std::unordered_map<TDF_Label, MaterialInfo> materialMap;
    using SubshapeLabelBuckets =
        std::unordered_map<TopoDS_TShape*, std::vector<TDF_Label>>;
    std::unordered_map<TDF_Label, SubshapeLabelBuckets> subshapeLabelCache;
    std::vector<Mesh> meshes;
    Standard_Size runtimeTessellationCount = 0;

    // for triangulation processing
    std::unordered_map<TopoDS_TShape*, ProcessedShapeInfo> processedShapeMap;
    // for remove edge duplication
    std::unordered_set<TopoDS_TShape*> processedEdgeSet;
    // for remove point duplication
    std::unordered_set<TopoDS_TShape*> processedPointSet;
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
                resolveShapeTree(shape, childLabel);
            }
        }
        
        processedShapeMap.clear();
        processedEdgeSet.clear();
        processedPointSet.clear();

        std::vector<TriGeometry> tris(triGeometryMap.size());
        for (const auto& [_, triInfo] : triGeometryMap) tris[triInfo.id] = std::move(triInfo.geometry);
        triGeometryMap.clear();
        std::vector<LineGeometry> lines(lineGeometryMap.size());
        for (const auto& [_, lineInfo] : lineGeometryMap) lines[lineInfo.id] = std::move(lineInfo.geometry);
        lineGeometryMap.clear();
        std::vector<PointGeometry> points(pointGeometryMap.size());
        for (const auto& [_, pointInfo] : pointGeometryMap) points[pointInfo.id] = std::move(pointInfo.geometry);
        pointGeometryMap.clear();
        std::vector<Material> materials(materialMap.size());
        for (const auto& [_, matInfo] : materialMap) materials[matInfo.id] = std::move(matInfo.material);
        materialMap.clear();
        subshapeLabelCache.clear();

        // clean up triangulation data to save memory
        for (TDF_ChildIterator it(shapeTool->Label()); it.More(); it.Next()) {
            TDF_Label childLabel = it.Value();
            TopoDS_Shape shape;
            if (shapeTool->GetShape(childLabel, shape) && shapeTool->IsFree(childLabel)) {
                BRepTools::Clean(shape, Standard_True);
            }
        }

        return TriangulatedModel(
            std::move(tris),
            std::move(lines),
            std::move(points),
            std::move(materials),
            std::move(meshes),
            runtimeTessellationCount
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

    std::string getLabelEntry(const TDF_Label& label) const {
        if (label.IsNull()) {
            return std::string();
        }
        TCollection_AsciiString entry;
        TDF_Tool::Entry(label, entry);
        return entry.ToCString();
    }

    std::string appendOccurrencePath(
        const std::string& parentPath,
        const TDF_Label& componentLabel
    ) const {
        const std::string entry = getLabelEntry(componentLabel);
        if (parentPath.empty()) {
            return entry;
        }
        return parentPath + "/" + entry;
    }

    TopoDS_Shape removeOccurrenceLocation(
        const TopoDS_Shape& shape,
        const gp_Trsf& occurrenceWorldTransform,
        const bool hasOccurrence
    ) const {
        if (!hasOccurrence) {
            return shape;
        }
        TopoDS_Shape definitionShape = shape;
        const gp_Trsf definitionTransform = occurrenceWorldTransform
            .Inverted()
            .Multiplied(shape.Location().Transformation());
        definitionShape.Location(TopLoc_Location(definitionTransform));
        return definitionShape;
    }

    bool transformsEquivalent(
        const gp_Trsf& left,
        const gp_Trsf& right
    ) const {
        for (Standard_Integer row = 1; row <= 3; ++row) {
            for (Standard_Integer column = 1; column <= 4; ++column) {
                if (
                    std::abs(left.Value(row, column) - right.Value(row, column))
                    > Precision::Confusion()
                ) {
                    return false;
                }
            }
        }
        return true;
    }

    bool shapesEquivalent(
        const TopoDS_Shape& left,
        const TopoDS_Shape& right
    ) const {
        if (left.IsNull() || right.IsNull()) {
            return false;
        }
        TopoDS_Shape unlocatedLeft = left;
        TopoDS_Shape unlocatedRight = right;
        unlocatedLeft.Location(TopLoc_Location());
        unlocatedRight.Location(TopLoc_Location());
        return (
            unlocatedLeft.IsPartner(unlocatedRight)
            && transformsEquivalent(
                left.Location().Transformation(),
                right.Location().Transformation()
            )
        );
    }

    const SubshapeLabelBuckets& getSubshapeLabelBuckets(
        const TDF_Label& definitionLabel
    ) {
        const auto [cacheIt, inserted] = subshapeLabelCache.try_emplace(
            definitionLabel
        );
        if (inserted) {
            TDF_LabelSequence subshapeLabels;
            if (shapeTool->GetSubShapes(definitionLabel, subshapeLabels)) {
                for (
                    Standard_Integer index = 1;
                    index <= subshapeLabels.Length();
                    ++index
                ) {
                    const TDF_Label candidateLabel = subshapeLabels.Value(index);
                    const TopoDS_Shape candidateShape = shapeTool->GetShape(
                        candidateLabel
                    );
                    if (!candidateShape.IsNull()) {
                        cacheIt->second[candidateShape.TShape().get()].push_back(
                            candidateLabel
                        );
                    }
                }
            }
        }
        return cacheIt->second;
    }

    TDF_Label findDefinitionShapeLabel(
        const TDF_Label& definitionLabel,
        const TopoDS_Shape& shape,
        const gp_Trsf& occurrenceWorldTransform,
        const bool hasOccurrence
    ) {
        if (definitionLabel.IsNull()) {
            return TDF_Label();
        }
        const TopoDS_Shape definitionShape = removeOccurrenceLocation(
            shape,
            occurrenceWorldTransform,
            hasOccurrence
        );
        const TopoDS_Shape definitionRoot = shapeTool->GetShape(definitionLabel);
        if (shapesEquivalent(definitionRoot, definitionShape)) {
            return definitionLabel;
        }
        const SubshapeLabelBuckets& buckets = getSubshapeLabelBuckets(
            definitionLabel
        );
        const auto candidates = buckets.find(definitionShape.TShape().get());
        if (candidates != buckets.end()) {
            for (const TDF_Label& candidateLabel : candidates->second) {
                if (shapesEquivalent(
                    shapeTool->GetShape(candidateLabel),
                    definitionShape
                )) {
                    return candidateLabel;
                }
            }
        }
        // Raw STEP and legacy XBF inputs may not materialize face labels. Keep
        // the OCCT lookup as a compatibility fallback, while canonical CATF
        // XBF resolves through the O(1) TShape bucket above.
        TDF_Label subshapeLabel;
        if (shapeTool->FindSubShape(
            definitionLabel,
            definitionShape,
            subshapeLabel
        )) {
            return subshapeLabel;
        }
        return TDF_Label();
    }

    TDF_Label findAssemblyComponent(
        const TDF_Label& assemblyDefinitionLabel,
        const TopoDS_Shape& childShape,
        const gp_Trsf& assemblyWorldTransform
    ) const {
        if (
            assemblyDefinitionLabel.IsNull()
            || !shapeTool->IsAssembly(assemblyDefinitionLabel)
        ) {
            return TDF_Label();
        }
        TopoDS_Shape relativeChild = childShape;
        const gp_Trsf relativeTransform = assemblyWorldTransform
            .Inverted()
            .Multiplied(childShape.Location().Transformation());
        relativeChild.Location(TopLoc_Location(relativeTransform));

        TDF_LabelSequence components;
        shapeTool->GetComponents(assemblyDefinitionLabel, components);
        for (Standard_Integer index = 1; index <= components.Length(); ++index) {
            const TDF_Label componentLabel = components.Value(index);
            const TopoDS_Shape componentShape = shapeTool->GetShape(componentLabel);
            if (shapesEquivalent(componentShape, relativeChild)) {
                return componentLabel;
            }
        }
        return TDF_Label();
    }

    std::vector<std::string> findFaceShapeLabels(
        const TopoDS_Shape& shape,
        const TDF_Label& definitionLabel,
        const gp_Trsf& occurrenceWorldTransform,
        const bool hasOccurrence
    ) {
        std::vector<std::string> labels;
        TopExp_Explorer faceExplorer(shape, TopAbs_FACE);
        for (; faceExplorer.More(); faceExplorer.Next()) {
            const TopoDS_Face& face = TopoDS::Face(faceExplorer.Current());
            TopLoc_Location location;
            const Handle(Poly_Triangulation) triangulation =
                BRep_Tool::Triangulation(face, location);
            if (!triangulation.IsNull()) {
                labels.push_back(getLabelEntry(findDefinitionShapeLabel(
                    definitionLabel,
                    face,
                    occurrenceWorldTransform,
                    hasOccurrence
                )));
            }
        }
        return labels;
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

    bool hasCompleteFaceTriangulation(const TopoDS_Shape& shape) const {
        Standard_Integer faceCount = 0;
        TopExp_Explorer faceExplorer(shape, TopAbs_FACE);
        for (; faceExplorer.More(); faceExplorer.Next()) {
            ++faceCount;
            const TopoDS_Face& face = TopoDS::Face(faceExplorer.Current());
            TopLoc_Location location;
            const Handle(Poly_Triangulation) triangulation =
                BRep_Tool::Triangulation(face, location);
            if (triangulation.IsNull() || triangulation->NbTriangles() <= 0) {
                return false;
            }
        }
        return faceCount > 0;
    }

    // shape must be TopoDS_Shell or TopoDS_Solid
    ProcessedShapeInfo triangulateShape(const TopoDS_Shape& shape) {
        // check if already processed
        if (processedShapeMap.find(shape.TShape().get()) != processedShapeMap.end()) {
            return processedShapeMap[shape.TShape().get()];
        }

        Standard_Integer triGeometryIndex = -1;
        Standard_Integer lineGeometryIndex = -1;
        Standard_Integer pointGeometryIndex = -1;

        gp_Trsf parentTransform = shape.Location().Transformation(); // parent global transform

        // from StdPrs_ToolTriangulatedShape::GetDeflection
        constexpr Standard_Real MAXIMAL_CHORDAL_DEVIATION = 0.0001;
        constexpr Standard_Real DEVIATION_COEFFICIENT = 0.001;
        Bnd_Box boundBox;
        BRepBndLib::Add(shape, boundBox, Standard_False);
        const Standard_Real deflection = Prs3d::GetDeflection(boundBox, DEVIATION_COEFFICIENT, MAXIMAL_CHORDAL_DEVIATION);
        
        constexpr Standard_Real ANGLE_DEFLECTION = 0.2;
        // XBF producers may persist a complete Poly_Triangulation display
        // cache. Reuse it verbatim so loading canonical CAD does not block the
        // browser on a second OCCT mesh pass. Legacy and third-party XBF files
        // remain supported through the existing local tessellation fallback.
        if (!hasCompleteFaceTriangulation(shape)) {
            ++runtimeTessellationCount;
            BRepMesh_IncrementalMesh mesh(
                shape, // The shape to mesh
                deflection, // Linear deflection
                Standard_True,  // Relative
                ANGLE_DEFLECTION, // Angular deflection
                Standard_True   // In parallel
            );
        }

        TriGeometry triData;
        LineGeometry lineData;
        PointGeometry pointData;

        TopExp_Explorer faceExplorer(shape, TopAbs_FACE);
        for (; faceExplorer.More(); faceExplorer.Next()) {
            const TopoDS_Face& face = TopoDS::Face(faceExplorer.Current());
            
            TopLoc_Location location;
            Handle(Poly_Triangulation) polyTri = BRep_Tool::Triangulation(face, location);

            if (!polyTri.IsNull()) {
                gp_Trsf childTransform = location.Transformation(); // child global transform
                gp_Trsf relativeTransform = parentTransform.Inverted().Multiplied(childTransform); // relative transform from parent to child
                TopAbs_Orientation faceOrientation = face.Orientation();
                Standard_Size indexOffset = static_cast<Standard_Size>(triData.positions.size() / 3);

                // triData.subMeshIndices.push_back(static_cast<uint32_t>(indexOffset)); // vertex start
                triData.subMeshIndices.push_back(static_cast<uint32_t>(polyTri->NbNodes())); // vertex count
                // triData.subMeshIndices.push_back(static_cast<uint32_t>(triData.indices.size())); // index start
                // triData.subMeshIndices.push_back(static_cast<uint32_t>(polyTri->NbTriangles() * 3)); // index count

                for (Standard_Integer i = 1; i <= polyTri->NbNodes(); ++i) {
                    gp_Pnt pnt = polyTri->Node(i).Transformed(relativeTransform);
                    triData.positions.push_back(static_cast<float>(pnt.X()));
                    triData.positions.push_back(static_cast<float>(pnt.Y()));
                    triData.positions.push_back(static_cast<float>(pnt.Z()));
                }

                BRepLib_ToolTriangulatedShape::ComputeNormals(face, polyTri);
                Standard_Boolean fixNormals = (faceOrientation == TopAbs_REVERSED) ^ (relativeTransform.VectorialPart().Determinant() < 0);
                if (fixNormals) {
                    for (Standard_Integer i = 1; i <= polyTri->NbNodes(); ++i) {
                        gp_Dir normal = polyTri->Normal(i).Reversed().Transformed(relativeTransform);
                        triData.normals.push_back(static_cast<float>(normal.X()));
                        triData.normals.push_back(static_cast<float>(normal.Y()));
                        triData.normals.push_back(static_cast<float>(normal.Z()));
                    }
                } else {
                    for (Standard_Integer i = 1; i <= polyTri->NbNodes(); ++i) {
                        gp_Dir normal = polyTri->Normal(i).Transformed(relativeTransform);
                        triData.normals.push_back(static_cast<float>(normal.X()));
                        triData.normals.push_back(static_cast<float>(normal.Y()));
                        triData.normals.push_back(static_cast<float>(normal.Z()));
                    }
                }

                Standard_Real umin, umax, vmin, vmax;
                BRepTools::UVBounds(face, umin, umax, vmin, vmax);
                for (Standard_Integer i = 1; i <= polyTri->NbNodes(); ++i) {
                    gp_Pnt2d uv = polyTri->UVNode(i);
                    float u = static_cast<float>((uv.X() - umin) / (umax - umin));
                    float v = static_cast<float>((uv.Y() - vmin) / (vmax - vmin));
                    triData.uvs.push_back(u);
                    triData.uvs.push_back(v);
                }

                if (faceOrientation == TopAbs_REVERSED) {
                    // reverse triangle winding order
                    for (Standard_Integer i = 1; i <= polyTri->NbTriangles(); ++i) {
                        Standard_Integer n1, n2, n3;
                        polyTri->Triangle(i).Get(n1, n2, n3);
                        // convert to zero-based index
                        triData.indices.push_back(static_cast<uint32_t>(n1 - 1 + indexOffset));
                        triData.indices.push_back(static_cast<uint32_t>(n2 - 1 + indexOffset));
                        triData.indices.push_back(static_cast<uint32_t>(n3 - 1 + indexOffset));
                    }
                } else {
                    for (Standard_Integer i = 1; i <= polyTri->NbTriangles(); ++i) {
                        Standard_Integer n1, n2, n3;
                        polyTri->Triangle(i).Get(n1, n2, n3);
                        // convert to zero-based index
                        triData.indices.push_back(static_cast<uint32_t>(n1 - 1 + indexOffset));
                        triData.indices.push_back(static_cast<uint32_t>(n3 - 1 + indexOffset));
                        triData.indices.push_back(static_cast<uint32_t>(n2 - 1 + indexOffset));
                    }
                }
            }

            // edge triangulation
            {
                TopExp_Explorer edgeExplorer(face, TopAbs_EDGE);
                for (; edgeExplorer.More(); edgeExplorer.Next()) {
                    TopoDS_Edge edge = TopoDS::Edge(edgeExplorer.Current());

                    // skip if already processed
                    if (processedEdgeSet.find(edge.TShape().get()) != processedEdgeSet.end()) {
                        continue;
                    }
                    processedEdgeSet.insert(edge.TShape().get());

                    gp_Trsf childTransform = edge.Location().Transformation();
                    gp_Trsf relativeTransform = parentTransform.Inverted().Multiplied(childTransform); // relative transform from parent to child

                    // note: we can not reuse Poly_Triangulation from face here because it's stateful
                    TopLoc_Location location;
                    Handle(Poly_Triangulation) polyTri = BRep_Tool::Triangulation(face, location);
                    if (!polyTri.IsNull()) {
                        Handle(Poly_PolygonOnTriangulation) polypolyTri = BRep_Tool::PolygonOnTriangulation(edge, polyTri, location);
                        if (!polypolyTri.IsNull()) {
                            if (polypolyTri->NbNodes() < 2) continue; // NOTE: this might be unreachable

                            // lineData.subMeshIndices.push_back(static_cast<uint32_t>(lineData.positions.size() / 3)); // vertex start
                            lineData.subMeshIndices.push_back(static_cast<uint32_t>((polypolyTri->NbNodes() - 1) * 2)); // vertex count

                            const TColStd_Array1OfInteger& nodes = polypolyTri->Nodes();
                            for (Standard_Integer i = nodes.Lower(); i < nodes.Upper(); ++i) {
                                gp_Pnt pnt1 = polyTri->Node(nodes.Value(i)).Transformed(relativeTransform);
                                gp_Pnt pnt2 = polyTri->Node(nodes.Value(i + 1)).Transformed(relativeTransform);
                                
                                lineData.positions.push_back(static_cast<float>(pnt1.X()));
                                lineData.positions.push_back(static_cast<float>(pnt1.Y()));
                                lineData.positions.push_back(static_cast<float>(pnt1.Z()));

                                lineData.positions.push_back(static_cast<float>(pnt2.X()));
                                lineData.positions.push_back(static_cast<float>(pnt2.Y()));
                                lineData.positions.push_back(static_cast<float>(pnt2.Z()));
                            }
                            continue;
                        }
                    }
                        
                    { // fallback to BRep curve sampling
                        edge.Location(TopLoc_Location(relativeTransform));

                        // fallback to curve sampling
                        BRepAdaptor_Curve curve(edge);
                        GCPnts_TangentialDeflection points(curve, ANGLE_DEFLECTION, deflection);
                        if (points.NbPoints() < 2) continue; // NOTE: this might be unreachable

                        // lineData.subMeshIndices.push_back(static_cast<uint32_t>(lineData.positions.size() / 3)); // vertex start
                        lineData.subMeshIndices.push_back(static_cast<uint32_t>((points.NbPoints() - 1) * 2)); // vertex count

                        for (Standard_Integer i = 1; i < points.NbPoints(); ++i) {
                            gp_Pnt pnt1 = points.Value(i);
                            gp_Pnt pnt2 = points.Value(i + 1);

                            lineData.positions.push_back(static_cast<float>(pnt1.X()));
                            lineData.positions.push_back(static_cast<float>(pnt1.Y()));
                            lineData.positions.push_back(static_cast<float>(pnt1.Z()));

                            lineData.positions.push_back(static_cast<float>(pnt2.X()));
                            lineData.positions.push_back(static_cast<float>(pnt2.Y()));
                            lineData.positions.push_back(static_cast<float>(pnt2.Z()));
                        }
                        continue;
                    }
                }
            }
        }

        // Vertex traversal for point geometry
        {
            TopExp_Explorer vertexExplorer(shape, TopAbs_VERTEX);
            for (; vertexExplorer.More(); vertexExplorer.Next()) {
                TopoDS_Vertex vertex = TopoDS::Vertex(vertexExplorer.Current());

                // skip if already processed
                if (processedPointSet.find(vertex.TShape().get()) != processedPointSet.end()) {
                    continue;
                }
                processedPointSet.insert(vertex.TShape().get());

                gp_Trsf childTransform = vertex.Location().Transformation();
                gp_Trsf relativeTransform = parentTransform.Inverted().Multiplied(childTransform); // relative transform from parent to child

                vertex.Location(TopLoc_Location(relativeTransform));

                gp_Pnt pnt = BRep_Tool::Pnt(vertex);
                pointData.positions.push_back(static_cast<float>(pnt.X()));
                pointData.positions.push_back(static_cast<float>(pnt.Y()));
                pointData.positions.push_back(static_cast<float>(pnt.Z()));
            }
        }

        if (!triData.positions.empty() && !triData.indices.empty()) {
            TriGeometryInfo newTriInfo = {
                .id = static_cast<Standard_UInteger>(triGeometryMap.size()),
                .geometry = std::move(triData)
            };
            const auto [triIt, triInserted] = triGeometryMap.emplace(shape.TShape().get(), std::move(newTriInfo));
            triGeometryIndex = static_cast<Standard_Integer>(triIt->second.id);
        }

        if (!lineData.positions.empty()) {
            LineGeometryInfo newLineInfo = {
                .id = static_cast<Standard_UInteger>(lineGeometryMap.size()),
                .geometry = std::move(lineData)
            };
            const auto [lineIt, lineInserted] = lineGeometryMap.emplace(shape.TShape().get(), std::move(newLineInfo));
            lineGeometryIndex = static_cast<Standard_Integer>(lineIt->second.id);
        }

        if (!pointData.positions.empty()) {
            PointGeometryInfo newPointInfo = {
                .id = static_cast<Standard_UInteger>(pointGeometryMap.size()),
                .geometry = std::move(pointData)
            };
            const auto [pointIt, pointInserted] = pointGeometryMap.emplace(shape.TShape().get(), std::move(newPointInfo));
            pointGeometryIndex = static_cast<Standard_Integer>(pointIt->second.id);
        }
        
        ProcessedShapeInfo processedInfo = {
            .triGeometryIndex = triGeometryIndex,
            .lineGeometryIndex = lineGeometryIndex,
            .pointGeometryIndex = pointGeometryIndex
        };
        processedShapeMap[shape.TShape().get()] = processedInfo;
        return processedInfo;
    }

    void resolveShapeTree(
        const TopoDS_Shape& rootShape,
        const TDF_Label& rootLabel
    ) {
        struct StackFrame {
            TopoDS_Shape shape;
            Standard_Integer parentMeshIndex;
            gp_Trsf parentWorldTransform;
            TDF_Label definitionLabel;
            std::string assemblyPath;
            std::string occurrencePath;
            gp_Trsf occurrenceWorldTransform;
            bool hasOccurrence;
        };
        std::vector<StackFrame> stack;
        stack.push_back({
            rootShape,
            -1,
            gp_Trsf(),
            resolveReferredShapeLabel(rootLabel),
            getLabelEntry(rootLabel),
            std::string(),
            gp_Trsf(),
            false
        });

        while (!stack.empty()) {
            auto [
                shape,
                parentMeshIndex,
                parentWorldTransform,
                definitionLabel,
                assemblyPath,
                occurrencePath,
                occurrenceWorldTransform,
                hasOccurrence
            ] = stack.back();
            stack.pop_back();

            Standard_Integer meshIndex = static_cast<Standard_Integer>(meshes.size());
            gp_Trsf shapeTransform = shape.Location().Transformation();
            
            gp_Trsf relativeTransform = parentWorldTransform.Inverted().Multiplied(shapeTransform);
            std::array<float, 16> matrixArray;
            for (Standard_Integer row = 1; row < 4; ++row) {
                for (Standard_Integer col = 1; col <= 4; ++col) {
                    matrixArray[(col - 1) * 4 + (row - 1)] = static_cast<float>(relativeTransform.Value(row, col));
                }
            }
            matrixArray[3] = 0.0f;
            matrixArray[7] = 0.0f;
            matrixArray[11] = 0.0f;
            matrixArray[15] = 1.0f;

            MeshShapeType shapeType;
            switch (shape.ShapeType()) {
            case TopAbs_COMPOUND: shapeType = MeshShapeType::Compound; break;
            case TopAbs_COMPSOLID: shapeType = MeshShapeType::Compsolid; break;
            case TopAbs_SOLID: shapeType = MeshShapeType::Solid; break;
            case TopAbs_SHELL: shapeType = MeshShapeType::Shell; break;
            case TopAbs_EDGE: shapeType = MeshShapeType::Edge; break;
            default: shapeType = MeshShapeType::Unknown; break;
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
            Standard_Integer pointGeometryIndex = -1;
            std::vector<std::string> faceShapeLabels;
            if (shape.ShapeType() == TopAbs_COMPOUND || shape.ShapeType() == TopAbs_COMPSOLID) {
                // resolve sub-shapes if compound shape
                TopoDS_Iterator it(shape);
                for (; it.More(); it.Next()) {
                    const TopoDS_Shape childShape = it.Value();
                    TDF_Label childDefinitionLabel = definitionLabel;
                    std::string childAssemblyPath = assemblyPath;
                    std::string childOccurrencePath = occurrencePath;
                    gp_Trsf childOccurrenceWorldTransform = occurrenceWorldTransform;
                    bool childHasOccurrence = hasOccurrence;

                    const TDF_Label componentLabel = findAssemblyComponent(
                        definitionLabel,
                        childShape,
                        shapeTransform
                    );
                    if (!componentLabel.IsNull()) {
                        childAssemblyPath = appendOccurrencePath(
                            assemblyPath,
                            componentLabel
                        );
                        childOccurrencePath = childAssemblyPath;
                        childDefinitionLabel = resolveReferredShapeLabel(componentLabel);
                        childOccurrenceWorldTransform = childShape
                            .Location()
                            .Transformation();
                        childHasOccurrence = true;
                    }
                    stack.push_back({
                        childShape,
                        meshIndex,
                        shapeTransform,
                        childDefinitionLabel,
                        std::move(childAssemblyPath),
                        std::move(childOccurrencePath),
                        childOccurrenceWorldTransform,
                        childHasOccurrence
                    });
                }
            } else if (shape.ShapeType() == TopAbs_SOLID || shape.ShapeType() == TopAbs_SHELL) {
                ProcessedShapeInfo processedInfo = triangulateShape(shape);
                triGeometryIndex = processedInfo.triGeometryIndex;
                lineGeometryIndex = processedInfo.lineGeometryIndex;
                pointGeometryIndex = processedInfo.pointGeometryIndex;
                if (triGeometryIndex >= 0) {
                    faceShapeLabels = findFaceShapeLabels(
                        shape,
                        definitionLabel,
                        occurrenceWorldTransform,
                        hasOccurrence
                    );
                }
            }

            const std::string shapeLabel = getLabelEntry(findDefinitionShapeLabel(
                definitionLabel,
                shape,
                occurrenceWorldTransform,
                hasOccurrence
            ));
            meshes.push_back(Mesh(
                std::move(shapeName),
                matrixArray,
                shapeType,
                triGeometryIndex,
                lineGeometryIndex,
                pointGeometryIndex,
                materialIndex,
                parentMeshIndex,
                std::move(occurrencePath),
                shapeLabel,
                std::move(faceShapeLabels)
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
