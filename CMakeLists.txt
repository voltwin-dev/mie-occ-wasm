cmake_minimum_required(VERSION 3.10)

project(mie-occ-wasm)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(TARGET mie-occ-wasm)
set(CMAKE_CONFIGURATION_TYPES Debug;Release)
set(CMAKE_NINJA_FORCE_RESPONSE_FILE 1 CACHE INTERNAL "")

cmake_path(SET OCCT_PATH ${CMAKE_SOURCE_DIR}/build/occt)

option(MIE_ENABLE_MULTITHREAD "Enable multithreading support" OFF)

set(MIE_INSTALL_PROFILENAME)
if (CMAKE_BUILD_TYPE STREQUAL "Release")
    string(APPEND MIE_INSTALL_PROFILENAME "release")
else()
    string(APPEND MIE_INSTALL_PROFILENAME "debug")
endif()
if (MIE_ENABLE_MULTITHREAD)
    string(APPEND MIE_INSTALL_PROFILENAME "-multi")
else()
    string(APPEND MIE_INSTALL_PROFILENAME "-single")
endif()

cmake_path(SET CMAKE_INSTALL_PREFIX NORMALIZE ${CMAKE_SOURCE_DIR}/../src/occ_wasm/${MIE_INSTALL_PROFILENAME})
cmake_path(RELATIVE_PATH OCCT_PATH BASE_DIRECTORY "${CMAKE_SOURCE_DIR}")

set(MIE_OCCT_TOOLKITS
    # Search from STEPCAFControl_Reader deps
    TKernel # for Standard.hxx
    TKXSBase # for XSControl_Reader.hxx
    TKDE # for DE_ShapeFixParameters.hxx
    TKG3d # for TopAbs_ShapeEnum.hxx
    TKShHealing # for ShapeProcess.hxx
    TKBRep # for TopTools_SequenceOfShape.hxx
    TKMath # for TopLoc_Location.hxx
    TKLCAF # for TDF_LabelSequence.hxx
    TKXCAF # for XCAFDoc_DataMapOfShapeLabel.hxx
    TKDESTEP # for STEPControl_Reader.hxx

    # Search from TDocStd_Document deps
    TKCDF # for CDM_Document.hxx
    
    # Search from BRep_Builder deps
    TKG2d # for Geom2d_Curve.hxx

    # Search from BRepLib_ToolTriangulatedShape deps
    TKTopAlgo # for BRepLib_ToolTriangulatedShape.hxx

    # Search from BRepMesh_IncrementalMesh deps
    TKMesh # for BRepMesh_IncrementalMesh.hxx

    # Additional Build Error Fixes
    TKGeomBase # for GeomProjLib.hxx
    TKGeomAlgo # for IntSurf_LineOn2S.hxx
    TKService # for Graphic3d_AlphaMode.hxx
    TKCAF # for TNaming_NamedShape.hxx
    TKVCAF # for TPrsStd_DriverTable.hxx
    TKBO # for IntTools_FClass2d.hxx
    TKV3d # for AIS_InteractiveContext.hxx
    TKHLR # for HLRBRep.hxx
    TKPrim # for BRepPrimAPI_MakeBox.hxx
)

include(${OCCT_PATH}/adm/cmake/occt_macros.cmake)

set(MIE_USED_OCCT_TOOLKIT_PACKAGES)
foreach(OCCT_TOOLKIT ${MIE_OCCT_TOOLKITS})
    EXTRACT_TOOLKIT_PACKAGES(${OCCT_PATH}/src ${OCCT_TOOLKIT} OCCT_PACKAGES)
    list(APPEND MIE_USED_OCCT_TOOLKIT_PACKAGES ${OCCT_PACKAGES})
endforeach()
list(REMOVE_DUPLICATES MIE_USED_OCCT_TOOLKIT_PACKAGES)

set(MIE_OCCT_SOURCE_FOLDERS)
set(MIE_OCCT_TOOLKIT_INCLUDE_FOLDERS)
foreach(OCCT_PACKAGE ${MIE_USED_OCCT_TOOLKIT_PACKAGES})
    list(APPEND MIE_OCCT_SOURCE_FOLDERS ${OCCT_PATH}/src/${OCCT_PACKAGE}/*.c*)
    list(APPEND MIE_OCCT_TOOLKIT_INCLUDE_FOLDERS ${OCCT_PATH}/src/${OCCT_PACKAGE})
endforeach()

include(${OCCT_PATH}/adm/cmake/version.cmake)
configure_file("${OCCT_PATH}/adm/templates/Standard_Version.hxx.in" "include/Standard_Version.hxx" @ONLY)
list(APPEND MIE_OCCT_TOOLKIT_INCLUDE_FOLDERS ${CMAKE_CURRENT_BINARY_DIR}/include)

file(GLOB MIE_OCCT_SOURCE_FILES ${MIE_OCCT_SOURCE_FOLDERS})

file(GLOB
    MIE_BINDING_SOURCES CONFIGURE_DEPENDS
    src/*.hpp
    src/*.cpp
)

source_group("Sources" FILES ${MIE_BINDING_SOURCES})
source_group("OCCT" FILES ${MIE_OCCT_SOURCE_FILES})

if(EMSCRIPTEN)
    set(MIE_SHARED_COMPILE_FLAGS)
    list(APPEND MIE_SHARED_COMPILE_FLAGS
        --cache .emscripten_cache
        -O3
        -flto
    )
    if (CMAKE_BUILD_TYPE STREQUAL "Release")
        list(APPEND MIE_SHARED_COMPILE_FLAGS
            -sDISABLE_EXCEPTION_CATCHING=1
        )
    else()
        list(APPEND MIE_SHARED_COMPILE_FLAGS
            -sDISABLE_EXCEPTION_CATCHING=0
        )
    endif()
    if (MIE_ENABLE_MULTITHREAD)
        list(APPEND MIE_SHARED_COMPILE_FLAGS
            -pthread
        )
    endif()

    add_library(occt STATIC ${MIE_OCCT_SOURCE_FILES})
    target_include_directories(occt PRIVATE ${MIE_OCCT_TOOLKIT_INCLUDE_FOLDERS})
    target_compile_options(occt PRIVATE
        ${MIE_SHARED_COMPILE_FLAGS}
        -Wno-character-conversion
        -Wno-deprecated-pragma
        -Wno-deprecated-declarations
        -DOCCT_NO_PLUGINS
    )

    add_executable(${TARGET} ${MIE_BINDING_SOURCES})
    target_include_directories(${TARGET} PRIVATE ${MIE_OCCT_TOOLKIT_INCLUDE_FOLDERS})
    target_compile_options(${TARGET} PRIVATE
        ${MIE_SHARED_COMPILE_FLAGS}
        -Wno-deprecated-declarations
    )
    target_link_libraries(${TARGET} PRIVATE occt)
    target_link_options(${TARGET} PRIVATE
        ${MIE_SHARED_COMPILE_FLAGS}
        -sMODULARIZE=1
        -sEXPORT_ES6=1
        -sSTACK_SIZE=8MB
        -sMAXIMUM_MEMORY=4GB
        -sALLOW_MEMORY_GROWTH=1
        -sENVIRONMENT=web
        --bind
        --emit-tsd "${TARGET}.d.ts"
    )
    if (MIE_ENABLE_MULTITHREAD)
        target_link_options(${TARGET} PRIVATE
            -sPTHREAD_POOL_SIZE='navigator.hardwareConcurrency'
        )
    else()
        target_link_options(${TARGET} PRIVATE
            -sINITIAL_HEAP=64MB
        )
    endif()

    install(TARGETS ${TARGET} DESTINATION ${CMAKE_INSTALL_PREFIX})
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${TARGET}.wasm DESTINATION ${CMAKE_INSTALL_PREFIX})
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${TARGET}.d.ts DESTINATION ${CMAKE_INSTALL_PREFIX})
endif()
