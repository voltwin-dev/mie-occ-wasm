// Copyright (c) 2025 SolverX Corporation
// This file is part of MIE OpenCascade WebAssembly Bindings.
//
// This library is free software; you can redistribute it and/or modify it under
// the terms of the GNU Lesser General Public License version 2.1 as published
// by the Free Software Foundation.

#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <cstdint>
#include <vector>
#include <streambuf>
#include <istream>

#include <STEPCAFControl_Reader.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <TDocStd_Document.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <XCAFDoc_ColorTool.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <TDF_Label.hxx>
#include <TDF_ChildIterator.hxx>
#include <TopoDS_Compound.hxx>
#include <BRep_Builder.hxx>
#include <Quantity_Color.hxx>
#include <TDataStd_Name.hxx>
#include <Standard_Failure.hxx>
#include <TopoDS_Iterator.hxx>
#include <TopExp_Explorer.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <BRep_Tool.hxx>
#include <TopoDS_Face.hxx>
#include <BRepLib_ToolTriangulatedShape.hxx>
#include <Poly_Triangulation.hxx>
#include <TopLoc_Location.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRepTools.hxx>

#include "common.hpp"

class ByteStreamBuf : public std::streambuf {
public:
    ByteStreamBuf(const uint8_t* data, std::size_t size) {
        char* p = const_cast<char*>(reinterpret_cast<const char*>(data));
        setg(p, p, p + size);
    }
};

class Cad2Mesh {
public:
    static bool stepToMesh(const Uint8Array& buffer) {
        std::vector<uint8_t> data = emscripten::convertJSArrayToNumberVector<uint8_t>(buffer);
        
        STEPCAFControl_Reader stepCafReader;
        stepCafReader.SetProductMetaMode(Standard_True);

        ByteStreamBuf byteStreamBuf(data.data(), data.size());
        std::istream stream(&byteStreamBuf);

        Handle(TDocStd_Document) doc = new TDocStd_Document("BinXCAF");
        try {
            OCC_CATCH_SIGNALS
            IFSelect_ReturnStatus status = stepCafReader.ReadStream("stp", stream);
            if (status != IFSelect_RetDone) {
                return false;
            }
            
            if (!stepCafReader.Transfer(doc)) {
                return false;
            }
        } catch (const Standard_Failure& e) {
            std::cerr << "STEP import error: " << e.GetMessageString() << std::endl;
            return false;
        }

        const TDF_Label mainLabel = doc->Main();
        Handle(XCAFDoc_ShapeTool) shapeTool = XCAFDoc_DocumentTool::ShapeTool(mainLabel);
        Handle(XCAFDoc_ColorTool) colorTool = XCAFDoc_DocumentTool::ColorTool(mainLabel);

        TDF_ChildIterator it(shapeTool->Label());
        for (; it.More(); it.Next()) {
            TDF_Label childLabel = it.Value();
            TopoDS_Shape shape;
            if (shapeTool->GetShape(childLabel, shape) && shapeTool->IsFree(childLabel)) {
                // free shapes (root nodes)
                resolveShapeTree(shape, shapeTool, colorTool);
            }
        }
        
        return true;
    }

private:
    static TDF_Label resolveReferredShapeLabel(
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

    static bool getLabelColor(
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

    static std::string getLabelName(
        const TDF_Label& label,
        Handle(XCAFDoc_ShapeTool)& shapeTool
    ) {
        Handle(TDataStd_Name) nameAttr;
        if (label.FindAttribute(TDataStd_Name::GetID(), nameAttr)) {
            Standard_Integer length = nameAttr->Get().Length();
            char* buffer = new char[length + 1];
            nameAttr->Get().ToUTF8CString(buffer);
            std::string result(buffer, length);
            delete[] buffer;
            return result;
        }
        return "";
    }

    static void resolveShapeTree(
        const TopoDS_Shape& rootShape,
        Handle(XCAFDoc_ShapeTool)& shapeTool,
        Handle(XCAFDoc_ColorTool)& colorTool
    ) {
        struct StackFrame {
            TopoDS_Shape shape;
            int level;
        };
        std::vector<StackFrame> stack;
        stack.push_back({ rootShape, 0 });

        while (!stack.empty()) {
            auto [shape, level] = stack.back();
            stack.pop_back();

            EM_ASM({
                console.log("Level: " + $0);
            }, level);

            TDF_Label shapeLabel;
            if (shapeTool->Search(shape, shapeLabel)) {
                TDF_Label resolvedLabel = resolveReferredShapeLabel(shapeLabel, shapeTool);
                std::string shapeName = getLabelName(resolvedLabel, shapeTool);
                if (!shapeName.empty()) {
                    EM_ASM({
                        console.log("Shape Name: " + UTF8ToString($0));
                    }, shapeName.c_str());
                } else {
                    EM_ASM({
                        console.log("No shape name.");
                    });
                }
                Quantity_Color color;
                if (getLabelColor(resolvedLabel, shapeTool, colorTool, color)) {
                    EM_ASM({
                        console.log("Shape Color: R=" + $0 + " G=" + $1 + " B=" + $2);
                    }, color.Red(), color.Green(), color.Blue());
                } else {
                    EM_ASM({
                        console.log("No shape color.");
                    });
                }
            }

            // resolve sub-shapes if compound shape
            if (shape.ShapeType() == TopAbs_COMPOUND || shape.ShapeType() == TopAbs_COMPSOLID) {
                EM_ASM({
                    console.log("Compound shape with " + $0 + " children.");
                }, shape.NbChildren());
                TopoDS_Iterator it(shape);
                for (; it.More(); it.Next()) {
                    stack.push_back({ it.Value(), level + 1 });
                }
            } else {
                EM_ASM({
                    console.log("Leaf shape type: " + $0);
                }, shape.ShapeType());
                
                constexpr double ANGLE_DEFLECTION = 0.2;
                BRepMesh_IncrementalMesh mesh(
                    shape, // The shape to mesh
                    0.002, // Linear deflection
                    true,  // Relative
                    ANGLE_DEFLECTION, // Angular deflection
                    true   // In parallel
                );

                TopExp_Explorer faceExplorer(shape, TopAbs_FACE);
                for (; faceExplorer.More(); faceExplorer.Next()) {
                    TopoDS_Face face = TopoDS::Face(faceExplorer.Current());
                    EM_ASM({
                        console.log("Face found.");
                    });
                    
                    TopLoc_Location location;
                    Handle(Poly_Triangulation) polyTri = BRep_Tool::Triangulation(face, location);
                    if (!polyTri.IsNull()) {
                        gp_Trsf shapeTransform = location.Transformation();
                        
                        for (Standard_Integer i = 1; i <= polyTri->NbNodes(); ++i) {
                            gp_Pnt pnt = polyTri->Node(i).Transformed(shapeTransform);
                            EM_ASM({
                                console.log("Vertex: " + $0 + ", " + $1 + ", " + $2);
                            }, pnt.X(), pnt.Y(), pnt.Z());
                        }

                        Standard_Boolean flipNormals = (face.Orientation() == TopAbs_REVERSED) ^ (shapeTransform.VectorialPart().Determinant() < 0);

                        BRepLib_ToolTriangulatedShape::ComputeNormals(face, polyTri);
                        for (Standard_Integer i = 1; i <= polyTri->NbNodes(); ++i) {
                            gp_Dir normal = polyTri->Normal(i).Transformed(shapeTransform);
                            EM_ASM({
                                console.log("Normal: " + $0 + ", " + $1 + ", " + $2);
                            }, normal.X(), normal.Y(), normal.Z());
                        }
                    }
                }

                TopTools_IndexedMapOfShape edgesIndexedMap;
                TopExp::MapShapes(shape, TopAbs_EDGE, edgesIndexedMap);
                EM_ASM({
                    console.log("Edges: " + $0);
                }, edgesIndexedMap.Extent());
            }
        }
    }
};

EMSCRIPTEN_BINDINGS(cad2mesh_module) {
    emscripten::class_<Cad2Mesh>("Cad2Mesh")
        .class_function("stepToMesh", &Cad2Mesh::stepToMesh);
}
