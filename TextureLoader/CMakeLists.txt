cmake_minimum_required(VERSION 3.20)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(Diligent-TextureLoader VERSION 1.0.0 LANGUAGES CXX)

set(TEXTURE_LOADER_SOURCES

        dxgiformat.h
        pch.h
        TextureLoaderImpl.hpp

        JPEGCodec.h
        PNGCodec.h
        SGILoader.h
        BCTools.h
        Image.h
        TextureLoader.h
        TextureUtilities.h

        BCTools.cpp
        DDSLoader.cpp
        JPEGCodec.cpp
        Image.cpp
        KTXLoader.cpp
        SGILoader.cpp
        PNGCodec.cpp
        STBImpl.cpp
        TextureLoaderImpl.cpp
        TextureUtilities.cpp
)

add_library(Diligent-TextureLoader STATIC ${TEXTURE_LOADER_SOURCES})
set_common_target_properties(Diligent-TextureLoader)

find_package(PNG REQUIRED)
find_package(Stb REQUIRED)
find_package(GeoTIFF CONFIG REQUIRED)
find_package(JPEG REQUIRED)

target_include_directories(Diligent-TextureLoader
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${Stb_INCLUDE_DIR}
        ${GEOTIFF_INCLUDE_DIR}
)

target_link_libraries(Diligent-TextureLoader
        PRIVATE
        Diligent-BuildSettings
        Diligent-Common
        Diligent-PlatformInterface
        Diligent-GraphicsEngineInterface
        Diligent-GraphicsAccessories
        Diligent-GraphicsTools
        PNG::PNG
        JPEG::JPEG
        ZLIB::ZLIB
        ${GEOTIFF_LIBRARIES}
)

set_target_properties(Diligent-TextureLoader PROPERTIES
        FOLDER DiligentTools
)

if (DILIGENT_INSTALL_TOOLS)
    install_tools_lib(Diligent-TextureLoader)
endif ()
