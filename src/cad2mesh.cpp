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
#include <iostream>

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
                resolveLabelRecursively(childLabel, shapeTool, colorTool);
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
        static const std::vector<XCAFDoc_ColorType> colorTypes = { XCAFDoc_ColorSurf, XCAFDoc_ColorCurv, XCAFDoc_ColorGen };
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

    static void resolveLabelRecursively(
        const TDF_Label& label,
        Handle(XCAFDoc_ShapeTool)& shapeTool,
        Handle(XCAFDoc_ColorTool)& colorTool,
        int level = 0
    ) {
        std::cout << "Level " << level << " Label Tag: " << label.Tag() << std::endl;

        TDF_Label resolvedLabel = resolveReferredShapeLabel(label, shapeTool);
        std::string name = getLabelName(resolvedLabel, shapeTool);
        if (!name.empty()) {
            std::cout << "Label Name: " << name << std::endl;
        } else {
            std::cout << "No label name found." << std::endl;
        }
        Quantity_Color color;
        if (getLabelColor(resolvedLabel, shapeTool, colorTool, color)) {
            std::cout << "Label Color: R=" << color.Red() << " G=" << color.Green() << " B=" << color.Blue() << std::endl;
        } else {
            std::cout << "No label color found." << std::endl;
        }

        bool readShape = false;
        {
            if (!label.HasChild()) {
                readShape = true;
            }
            TDF_ChildIterator it(label);
            for (; it.More(); it.Next()) {
                TDF_Label childLabel = it.Value();
                
                if (shapeTool->IsSubShape(childLabel)) {
                    readShape = true;
                    break;
                }
                
                TopoDS_Shape childShape;
                if (shapeTool->GetShape(childLabel, childShape) && shapeTool->IsFree(childLabel)) {
                    readShape = true;
                    break;
                }
            }
        }

        if (readShape) {
            TopoDS_Shape shape = shapeTool->GetShape(label);
            resolveShapeRecursively(shape, shapeTool, colorTool);
        } else {
            TDF_ChildIterator it(label);
            for (; it.More(); it.Next()) {
                TDF_Label childLabel = it.Value();

                std::cout << "Child Label Tag: " << childLabel.Tag() << std::endl;

                TopoDS_Shape childShape;
                if (shapeTool->GetShape(childLabel, childShape) && shapeTool->IsFree(childLabel)) {
                    std::cout << "Free Shape Found?" << std::endl;
                    resolveLabelRecursively(childLabel, shapeTool, colorTool, level + 1);
                }
            }
        }
    }

    static void resolveShapeRecursively(
        const TopoDS_Shape& shape,
        Handle(XCAFDoc_ShapeTool)& shapeTool,
        Handle(XCAFDoc_ColorTool)& colorTool
    ) {
        TDF_Label shapeLabel;
        if (shapeTool->Search(shape, shapeLabel)) {
            TDF_Label resolvedLabel = resolveReferredShapeLabel(shapeLabel, shapeTool);
            std::string shapeName = getLabelName(resolvedLabel, shapeTool);
            if (!shapeName.empty()) {
                std::cout << "Shape Name: " << shapeName << std::endl;
            } else {
                std::cout << "No shape name found." << std::endl;
            }
            Quantity_Color color;
            if (getLabelColor(resolvedLabel, shapeTool, colorTool, color)) {
                std::cout << "Shape Color: R=" << color.Red() << " G=" << color.Green() << " B=" << color.Blue() << std::endl;
            } else {
                std::cout << "No shape color found." << std::endl;
            }
        }

        if (shape.ShapeType() == TopAbs_COMPOUND || shape.ShapeType() == TopAbs_COMPSOLID) {
            std::cout << "Compound shape with " << shape.NbChildren() << " children." << std::endl;
            TopoDS_Iterator it(shape);
            for (; it.More(); it.Next()) {
                TopoDS_Shape subShape = it.Value();
                resolveShapeRecursively(subShape, shapeTool, colorTool);
            }
        } else {
            std::cout << "Leaf shape type: " << shape.ShapeType() << std::endl;
        }
    }
};

EMSCRIPTEN_BINDINGS(cad2mesh_module) {
    emscripten::class_<Cad2Mesh>("Cad2Mesh")
        .class_function("stepToMesh", &Cad2Mesh::stepToMesh);
}
