cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(Diligent-TextureLoader VERSION 1.0.0 LANGUAGES CXX)

set(INCLUDE
        include/dxgiformat.h
        include/pch.h
        include/TextureLoaderImpl.hpp
)

set(INTERFACE
        interface/JPEGCodec.h
        interface/PNGCodec.h
        interface/SGILoader.h
        interface/BCTools.h
        interface/Image.h
        interface/TextureLoader.h
        interface/TextureUtilities.h
)

set(SOURCE
        src/BCTools.cpp
        src/DDSLoader.cpp
        src/JPEGCodec.cpp
        src/Image.cpp
        src/KTXLoader.cpp
        src/SGILoader.cpp
        src/PNGCodec.cpp
        src/STBImpl.cpp
        src/TextureLoaderImpl.cpp
        src/TextureUtilities.cpp
)

add_library(Diligent-TextureLoader STATIC ${SOURCE} ${INCLUDE} ${INTERFACE})
set_common_target_properties(Diligent-TextureLoader)

set_property(SOURCE src/PNGCodec.cpp src/Image.cpp
        APPEND PROPERTY INCLUDE_DIRECTORIES
        "${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/libpng" # png_static target does not define any public include directories
        "${CMAKE_CURRENT_BINARY_DIR}/../ThirdParty/libpng" # pnglibconf.h is generated in the binary directory
)

find_package(PNG REQUIRED)
find_package(Stb REQUIRED)

target_include_directories(Diligent-TextureLoader
        PUBLIC
        interface
        PRIVATE
        include
        ${Stb_INCLUDE_DIR}
)

source_group("source" FILES ${SOURCE})
source_group("include" FILES ${INCLUDE})
source_group("interface" FILES ${INTERFACE})


target_link_libraries(Diligent-TextureLoader
        PRIVATE
        Diligent-BuildSettings
        Diligent-Common
        Diligent-PlatformInterface
        Diligent-GraphicsEngineInterface
        Diligent-GraphicsAccessories
        Diligent-GraphicsTools
        PNG::PNG
        TIFF::TIFF
        ZLIB::ZLIB
)

if (NOT DILIGENT_EXTERNAL_LIBJPEG)
    target_link_libraries(Diligent-TextureLoader
            PRIVATE
            JPEG::JPEG
    )
else ()
    target_link_libraries(Diligent-TextureLoader
            PRIVATE
            ${DILIGENT_EXTERNAL_LIBJPEG}
    )
endif ()

set_target_properties(Diligent-TextureLoader PROPERTIES
        FOLDER DiligentTools
)

if (DILIGENT_INSTALL_TOOLS)
    install_tools_lib(Diligent-TextureLoader)
endif ()
