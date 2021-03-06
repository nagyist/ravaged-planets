cmake_minimum_required (VERSION 3.1)

# The eclipse generator still doesn't generate the correct values for __cplusplus unless I also do this.
# We need to run this BEFORE project() because the command project() runs to determine compiler support
# will not run it otherwise.
if(${CMAKE_EXTRA_GENERATOR} MATCHES "Eclipse CDT4")
    set(CMAKE_CXX_COMPILER_ARG1 "-std=c++11" CACHE STRING "C++ version for eclipse" FORCE)
    set(CMAKE_ECLIPSE_VERSION "4.4" CACHE STRING "Eclipse version" FORCE)
endif()

project (RavagedPlanets)

# Add our custom CMAKE modules
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules")

# Make sure we compile with C++11 features
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_EXTENSIONS off)
set(CMAKE_CXX_STANDARD_REQUIRED on)

if (CMAKE_BUILD_TYPE MATCHES Debug)
    add_definitions(-DDEBUG)
endif()

if(WIN32)
    # The following command changes \ to / in the Program Files Path so CMake will not complain about bad escape
    # sequences.
    string (REPLACE "\\" "/" PGM_FILES $ENV{PROGRAMFILES})

    set (CMAKE_INSTALL_PREFIX ${PGM_FILES}/Codeka/Ravaged-Planets
         CACHE STRING "Default Install Path" FORCE)
endif()

if(MSVC)
    # Set /EHsc so we can have proper unwind semantics in Visual C++
    add_definitions(/EHsc)
    if (CMAKE_BUILD_TYPE MATCHES Debug)
        # /MDd = multi-threaded debug DLL
        add_definitions(/MDd)
        add_definitions(/ZI)
        add_definitions(/Od)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /DEBUG")
    endif()
    add_definitions(/MP)
endif()

if (CMAKE_COMPILER_IS_GNUCC)
    # Make sure we compile with -rdynamic so that stack traces can include method names.
    add_definitions(-rdynamic)
endif()

# Make sure we can include our own include files.
include_directories(include)

# Make sure generated files are #includable.
include_directories(${CMAKE_CURRENT_BINARY_DIR}/include)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/src)

# Look for the various libraries we depend on.
find_package(Boost COMPONENTS filesystem iostreams program_options signals system REQUIRED)
find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(Protobuf REQUIRED)
find_package(Lua REQUIRED)
find_package(Luabind REQUIRED)
find_package(CURL REQUIRED)
find_package(SDL2 REQUIRED)
find_package(SDL_mixer REQUIRED)
find_package(Freetype REQUIRED)
find_package(ENet REQUIRED)
find_package(Assimp) # not required, no meshexp if not found.

if(LINUX)
    # I don't know why, but libSDL is importing symbols from libX11.
    find_package(X11 REQUIRED)
    list(APPEND SDL2_LIBRARY ${X11_LIBRARIES} pthread)
elseif(APPLE)
    find_package(BZip2 REQUIRED)
endif()

include_directories(${Boost_INCLUDE_DIR})
if (CMAKE_BUILD_TYPE MATCHES Debug)
    link_directories(${Boost_LIBRARY_DIR_DEBUG})
else()
    link_directories(${Boost_LIBRARY_DIR_RELEASE})
endif()
include_directories(${SDL2_INCLUDE_DIR})
include_directories(${SDL_mixer_INCLUDE_DIR})
include_directories(${OPENGL_INCLUDE_DIR})
include_directories(${ENET_INCLUDE_DIR})
include_directories(${FREETYPE_INCLUDE_DIR_ft2build})
include_directories(${GLEW_INCLUDE_DIR})
include_directories(${PROTOBUF_INCLUDE_DIR})
include_directories(${LUA_INCLUDE_DIR})
include_directories(${LUABIND_INCLUDE_DIR})
include_directories(${CURL_INCLUDE_DIR})

# TODO: not sure if there's an automatic way to detect whether these are static?
if (MSVC AND CMAKE_BUILD_TYPE MATCHES Debug)
    add_definitions(-DGLEW_STATIC)
    add_definitions(-DCURL_STATICLIB)
    add_definitions(-DSDL_STATIC)
endif()

add_subdirectory(src/version-number)
add_subdirectory(src/framework)
add_subdirectory(src/meshexp)
add_subdirectory(src/font-test)
add_subdirectory(src/lua-test)
add_subdirectory(src/particle-test)
add_subdirectory(src/mesh-test)
add_subdirectory(src/game)

# Be sure to install the "data" directory into /share/war-worlds
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/data/"
        DESTINATION share/ravaged-planets
        REGEX "\\.git" EXCLUDE
)
