# This is the top-level CMakeLists.txt file for the Clazy project.
#
# To build the man page from POD, run 'make man' after CMake (assumes perl is available)
# To install the resulting man page, run 'make install'
# The man page is not available on Windows.
#

project(ClangLazy)

cmake_minimum_required(VERSION 3.0)
include(FeatureSummary)
include(GenerateExportHeader)
include(ClazySources.cmake)

# Version setup
set(CLAZY_VERSION_MAJOR "1")
set(CLAZY_VERSION_MINOR "2")
set(CLAZY_VERSION_PATCH "0")
set(CLAZY_VERSION "${CLAZY_VERSION_MAJOR}.${CLAZY_VERSION_MINOR}.${CLAZY_VERSION_PATCH}")
set(CLAZY_PRINT_VERSION "${CLAZY_VERSION_MAJOR}.${CLAZY_VERSION_MINOR}")

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake)
find_package(Clang 3.7 MODULE REQUIRED)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

add_definitions(-D__STDC_LIMIT_MACROS -D__STDC_CONSTANT_MACROS)
add_definitions(-D_GNU_SOURCE -DHAVE_CLANG_CONFIG_H)

option(CLAZY_INSTALL_NO_HEADERS "Enable this option to suppress installation of headers" OFF)

if (MSVC AND NOT CLANG_LIBRARY_IMPORT)
  message(FATAL_ERROR "\nOn MSVC you need to pass -DCLANG_LIBRARY_IMPORT=C:/path/to/llvm-build/lib/clang.lib to cmake when building Clazy.\nAlso make sure you've built LLVM with -DLLVM_EXPORT_SYMBOLS_FOR_PLUGINS=ON")
endif()

if(MSVC)
  # disable trigger-happy warnings from Clang/LLVM headers
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4267 /wd4244 /wd4291 /wd4800 /wd4141 /wd4146 /wd4251")
elseif(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fno-common -Woverloaded-virtual -Wcast-qual -fno-strict-aliasing -pedantic -Wno-long-long -Wall -W -Wno-unused-parameter -Wwrite-strings -fno-exceptions -fno-rtti")
endif()

set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -Wl,-flat_namespace -Wl,-undefined -Wl,suppress")
if(WIN32)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
else()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
endif()

# Look for std::regex support
message("Looking for std::regex support...")
try_run(RUN_RESULT COMPILE_RESULT ${CMAKE_BINARY_DIR} ${CMAKE_SOURCE_DIR}/cmake_has_regex_test.cpp)

if(RUN_RESULT EQUAL 0)
  set(HAS_STD_REGEX TRUE)
else()
  set(HAS_STD_REGEX FALSE)
endif()

string(REPLACE " " ";" LLVM_LIBS_LIST ${LLVM_LIBS}) # Transform into a list

if(NOT APPLE)
  # Don't link against LLVMSupport, causes: CommandLine Error: Option 'view-background' registered more than once!
  list(REMOVE_ITEM LLVM_LIBS_LIST "-lLLVMSupport")    # Remove element
endif()

if (MSVC)
  message("REMOVES ${CLANG_LIBS}" )
  list(REMOVE_ITEM CLANG_LIBS "-lFrontend")
endif()

macro(link_to_llvm name)
  foreach(clang_lib ${CLANG_LIBS})
    if (MSVC)
        get_filename_component(LIB_FILENAME ${clang_lib} NAME)
        if (LIB_FILENAME STREQUAL "clangFrontend.lib")
            # On MSVC we don't link against clangFrontend.lib, instead we link against clang.exe (via clang.lib)
            # Otherwise the clazy plugin would have it's own plugin registry and clang wouldn't see it.
            # This way clazy registers with clang.
            continue()
        endif()
    endif()

    target_link_libraries(${name} ${clang_lib})
  endforeach()

  foreach(llvm_lib ${LLVM_LIBS_LIST})
    target_link_libraries(${name} ${llvm_lib})
  endforeach()

  foreach(user_lib ${USER_LIBS})
    target_link_libraries(${name} ${user_lib})
  endforeach()

  foreach(llvm_system_lib ${LLVM_SYSTEM_LIBS})
    target_link_libraries(${name} ${llvm_system_lib})
  endforeach()

endmacro()

macro(add_clang_plugin name)
  set(srcs ${ARGN})

  include_directories(${CLANG_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src)
  link_directories("${LLVM_INSTALL_PREFIX}/lib" ${LLVM_LIBRARY_DIRS})

  add_library(${name} SHARED ${srcs})

  if(SYMBOL_FILE)
    set_target_properties(${name} PROPERTIES LINK_FlAGS "-exported_symbols_list ${SYMBOL_FILE}")
  endif()

  link_to_llvm(${name})

  if(MSVC)
    target_link_libraries(${name} version.lib)
    target_link_libraries(${name} ${CLANG_LIBRARY_IMPORT}) # Link against clang.exe to share the plugin registry
  endif()

  target_link_libraries(${name} clazylib)
endmacro()

# clazylib version
set(clazylib_VERSION_MAJOR 0)
set(clazylib_VERSION_MINOR 1)
# Enable the full x.y.z version only for release versions
set(clazylib_VERSION_PATCH 0)
set(clazylib_VERSION ${clazylib_VERSION_MAJOR}.${clazylib_VERSION_MINOR})

include_directories(${CMAKE_BINARY_DIR})

link_directories("${LLVM_INSTALL_PREFIX}/lib" ${LLVM_LIBRARY_DIRS})
add_library(clazylib SHARED ${CLAZY_LIB_SRC})
link_to_llvm(clazylib)

generate_export_header(clazylib)

set_target_properties(clazylib PROPERTIES VERSION ${clazylib_VERSION} SOVERSION ${clazylib_VERSION_MAJOR})
include("GNUInstallDirs")

install(TARGETS clazylib EXPORT LibClaryExport
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

set(SYMBOL_FILE Lazy.exports)

if(HAS_STD_REGEX)
  #level2
  set(CLAZY_SRCS ${CLAZY_SRCS} src/checks/level2/oldstyleconnect.cpp)
else()
  add_definitions(-DNO_STD_REGEX)
  message("old-style-connect check is disabled due to missing std::regex support")
  message("Suppressions are disabled due to missing std::regex support")
endif()

add_clang_plugin(ClangLazy ${CLAZY_SRCS})

set_target_properties(ClangLazy PROPERTIES
  LINKER_LANGUAGE CXX
  PREFIX ""
)

install(TARGETS ClangLazy
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

set(SHARE_INSTALL_DIR ${CMAKE_INSTALL_DATAROOTDIR} CACHE STRING "Share directory name")

if(NOT WIN32)
  configure_file(${CMAKE_SOURCE_DIR}/clazy.cmake ${CMAKE_BINARY_DIR}/clazy @ONLY)
  install(FILES ${CMAKE_BINARY_DIR}/clazy DESTINATION bin PERMISSIONS OWNER_WRITE OWNER_EXECUTE OWNER_READ GROUP_EXECUTE GROUP_READ WORLD_READ WORLD_EXECUTE)
else()
  install(FILES ${CMAKE_BINARY_DIR}/clazy.bat DESTINATION bin PERMISSIONS OWNER_WRITE OWNER_EXECUTE OWNER_READ GROUP_EXECUTE GROUP_READ WORLD_READ WORLD_EXECUTE)
  install(FILES ${CMAKE_BINARY_DIR}/clazy-cl.bat DESTINATION bin PERMISSIONS OWNER_WRITE OWNER_EXECUTE OWNER_READ GROUP_EXECUTE GROUP_READ WORLD_READ WORLD_EXECUTE)
endif()

# Install the explanation README's
set(DOC_INSTALL_DIR ${SHARE_INSTALL_DIR}/clazy/doc)
file(GLOB README_LEVEL0_FILES src/checks/level0/README-*)
file(GLOB README_LEVEL1_FILES src/checks/level1/README-*)
file(GLOB README_LEVEL2_FILES src/checks/level2/README-*)
file(GLOB README_LEVEL3_FILES src/checks/level3/README-*)
file(GLOB README_HIDDENLEVEL_FILES src/checks/hiddenlevel/README-*)
install(FILES ${README_LEVEL0_FILES} DESTINATION ${DOC_INSTALL_DIR}/level0)
install(FILES ${README_LEVEL1_FILES} DESTINATION ${DOC_INSTALL_DIR}/level1)
install(FILES ${README_LEVEL2_FILES} DESTINATION ${DOC_INSTALL_DIR}/level2)
install(FILES ${README_LEVEL3_FILES} DESTINATION ${DOC_INSTALL_DIR}/level3)
install(FILES ${README_HIDDENLEVEL_FILES} DESTINATION ${DOC_INSTALL_DIR}/hiddenlevel)

# Install more doc files
install(FILES README.md LICENSE.GPL.txt DESTINATION ${DOC_INSTALL_DIR})

# Build docs
set(MAN_INSTALL_DIR "man/man1")
add_subdirectory(docs)

if (NOT CLAZY_INSTALL_NO_HEADERS)
  # Install public headers
  set(CLAZY_LIB_INCLUDES
    src/AccessSpecifierManager.h
    src/checkbase.h
    src/checkmanager.h
    ${CMAKE_BINARY_DIR}/clazylib_export.h
    src/clazy_stl.h
    src/ContextUtils.h
    src/FixItUtils.h
    src/HierarchyUtils.h
    src/LoopUtils.h
    src/MacroUtils.h
    src/QtUtils.h
    src/SuppressionManager.h
    src/StmtBodyRange.h
    src/StringUtils.h
    src/TemplateUtils.h
    src/TypeUtils.h
    src/Utils.h
  )
  install(FILES ${CLAZY_LIB_INCLUDES} DESTINATION include/clazy)
endif()
