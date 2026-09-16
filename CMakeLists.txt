cmake_minimum_required(VERSION 3.14)
project(typesafe-sdk-cpp VERSION 0.1.0 LANGUAGES CXX)

include(CTest)

# ------------------------------------------------------------------------------
# Dependencies
# ------------------------------------------------------------------------------
find_package(nlohmann_json REQUIRED)

option(TYPESAFE_USE_LIBCURL "Build the default libcurl transport" ON)
if(TYPESAFE_USE_LIBCURL)
    find_package(CURL REQUIRED)
endif()

option(TYPESAFE_USE_BOOST_ASIO "Build the optional Boost.Beast HTTPS transport" OFF)
if(TYPESAFE_USE_BOOST_ASIO)
    find_package(Boost REQUIRED COMPONENTS system)
    find_package(OpenSSL REQUIRED)
endif()

# ------------------------------------------------------------------------------
# Core Library Target
# ------------------------------------------------------------------------------
add_library(typesafe_cpp
    src/types.cpp
    src/client.cpp
)
add_library(typesafe::typesafe ALIAS typesafe_cpp)

target_compile_features(typesafe_cpp PUBLIC cxx_std_20)

target_include_directories(typesafe_cpp PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(typesafe_cpp PUBLIC nlohmann_json::nlohmann_json)

target_compile_definitions(typesafe_cpp PRIVATE
    TYPESAFE_SDK_VERSION="${PROJECT_VERSION}"
)

if(TYPESAFE_USE_LIBCURL)
    target_sources(typesafe_cpp PRIVATE src/curl_transport.cpp)
    target_compile_definitions(typesafe_cpp PUBLIC TYPESAFE_USE_LIBCURL)
    target_link_libraries(typesafe_cpp PUBLIC CURL::libcurl)
endif()

if(TYPESAFE_USE_BOOST_ASIO)
    target_sources(typesafe_cpp PRIVATE src/boost_transport.cpp)
    target_compile_definitions(typesafe_cpp PUBLIC TYPESAFE_USE_BOOST_ASIO)
    target_link_libraries(typesafe_cpp PUBLIC Boost::system OpenSSL::SSL OpenSSL::Crypto)
endif()

# ------------------------------------------------------------------------------
# Testing
# ------------------------------------------------------------------------------
if(BUILD_TESTING)
    include(FetchContent)
    FetchContent_Declare(googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.15.2
        GIT_SHALLOW    TRUE
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        if(TARGET gtest)
            target_compile_options(gtest PRIVATE -Wno-character-conversion)
        endif()
        if(TARGET gmock)
            target_compile_options(gmock PRIVATE -Wno-character-conversion)
        endif()
    endif()

    add_executable(typesafe_sdk_tests
        tests/types_test.cpp
        tests/client_test.cpp
        tests/async_test.cpp
    )
    target_link_libraries(typesafe_sdk_tests PRIVATE 
        GTest::gtest_main 
        typesafe_cpp
    )
    
    include(GoogleTest)
    gtest_discover_tests(typesafe_sdk_tests)
endif()

# ------------------------------------------------------------------------------
# Examples
# ------------------------------------------------------------------------------
option(TYPESAFE_BUILD_EXAMPLES "Build the example programs" ON)
if(TYPESAFE_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
