cmake_minimum_required (VERSION 3.10)

if(TARGET gtest)
    if(DILIGENT_BUILD_TOOLS_TESTS)
        add_subdirectory(DiligentToolsTest)
        add_subdirectory(DiligentToolsGPUTest)
    endif()
endif()