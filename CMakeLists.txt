cmake_minimum_required(VERSION 3.17)

option(TESTS_WITH_ASAN  "Enable AddressSanitizer on the test targets"   ON)

# A numeric verbosity level can be set for debug output printed/logged during execution
# 0: Nothing
# 1: Model info (source code)
# 2: BMI info (e.g.current timestep)
# Use 1 by default when build type is "Debug"
if (NOT DEFINED DEBUG_VERBOSITY AND "${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    set(DEBUG_VERBOSITY 1)
# Use 0 by default for any build types other than "Debug"
elseif (NOT DEFINED DEBUG_VERBOSITY)
    set(DEBUG_VERBOSITY 0)
# Catch any "ON" or equivalent, excluding all integers other than "1" (which we want to handle separately)
elseif (${DEBUG_VERBOSITY} AND NOT ${DEBUG_VERBOSITY} GREATER "1" AND NOT ${DEBUG_VERBOSITY} LESS_EQUAL "0")
    set(DEBUG_VERBOSITY 1)
# Should catch any other values except an integer greater than 1 (which we want to use directly)
elseif (NOT ${DEBUG_VERBOSITY} GREATER "1")
    set(DEBUG_VERBOSITY 0)
endif ()
# While not an explicit condition, DEBUG_VERBOSITY is left alone if set to some integer greater than 1

add_compile_definitions(TOPMODEL_DEBUG=${DEBUG_VERBOSITY})

project(topmodelbmi VERSION 1.0.0 DESCRIPTION "OWP TOPMODEL Model and BMI Module Shared Library")

set(TOPMODEL_LIB_NAME_CMAKE topmodelbmi)
set(TOPMODEL_LIB_DESC_CMAKE "OWP TOPMODEL Model and BMI Module Shared Library")

####################
# Some global properties
set_property(GLOBAL PROPERTY C_STANDARD 99)      # Note that code requires minimum of C99 standard to compile
set_property(GLOBAL PROPERTY C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)                       # State serialization is implemented in C++
set(CMAKE_CXX_STANDARD_REQUIRED ON)
####################

####################
# Boost, for state serialization.  Version floor matches ngen's.
if (POLICY CMP0144)
    cmake_policy(SET CMP0144 NEW)    # Honor the BOOST_ROOT environment variable
endif ()
if (POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)    # Prefer Boost's own config over the removed FindBoost module
endif ()
# A Boost built in place with b2 stages its libraries and CMake config files under
# <BOOST_ROOT>/stage, which is not a prefix find_package would search on its own.
if (DEFINED ENV{BOOST_ROOT} AND IS_DIRECTORY "$ENV{BOOST_ROOT}/stage/lib/cmake")
    list(APPEND CMAKE_PREFIX_PATH "$ENV{BOOST_ROOT}/stage")
endif ()
set(Boost_USE_STATIC_LIBS OFF)
find_package(Boost 1.86.0 REQUIRED COMPONENTS serialization)
####################

####################
# Stuff for shared library
if(WIN32)
    add_library(topmodelbmi src/bmi_topmodel.c src/topmodel.c src/bmi_serialization.cpp)
else()
    add_library(topmodelbmi SHARED src/bmi_topmodel.c src/topmodel.c src/bmi_serialization.cpp)
endif()

target_compile_definitions(topmodelbmi PRIVATE BMI_ACTIVE)
target_include_directories(topmodelbmi PRIVATE include)
target_link_libraries(topmodelbmi m Boost::serialization)
set_target_properties(topmodelbmi PROPERTIES
        VERSION ${PROJECT_VERSION}
        PUBLIC_HEADER include/bmi_topmodel.h
)
####################

####################
# Stuff for standalone driver executable
add_executable(topmodeldriver src/main.c src/bmi_topmodel.c src/topmodel.c src/bmi_serialization.cpp)
target_include_directories(topmodeldriver PRIVATE include)
target_link_libraries(topmodeldriver m Boost::serialization)
####################

####################
# Stuff for simple test routine
add_executable(simple_unit_tests test/main_unit_test_bmi.c src/bmi_topmodel.c src/topmodel.c src/bmi_serialization.cpp)
target_compile_definitions(simple_unit_tests PRIVATE REPO_ROOT_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(simple_unit_tests PRIVATE include)
target_link_libraries(simple_unit_tests m Boost::serialization)
if (TESTS_WITH_ASAN)
    # Seems this should work in all compiler
    target_compile_options(simple_unit_tests PRIVATE -fsanitize=address)
    target_link_options(simple_unit_tests PRIVATE -fsanitize=address)
endif ()
####################

####################
# Stuff for ngen serialization protocol conformance tests
add_executable(serialization_unit_tests test/main_unit_test_serialization.c src/bmi_topmodel.c src/topmodel.c src/bmi_serialization.cpp)
target_compile_definitions(serialization_unit_tests PRIVATE REPO_ROOT_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(serialization_unit_tests PRIVATE include)
target_link_libraries(serialization_unit_tests m Boost::serialization)
if (TESTS_WITH_ASAN)
    target_compile_options(serialization_unit_tests PRIVATE -fsanitize=address)
    target_link_options(serialization_unit_tests PRIVATE -fsanitize=address)
endif ()
####################

####################
# Stuff for long-run restart equivalence tests; run separately from the others
add_executable(serialization_integration_tests test/main_integration_test_serialization.c src/bmi_topmodel.c src/topmodel.c src/bmi_serialization.cpp)
target_compile_definitions(serialization_integration_tests PRIVATE REPO_ROOT_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(serialization_integration_tests PRIVATE include)
target_link_libraries(serialization_integration_tests m Boost::serialization)
if (TESTS_WITH_ASAN)
    target_compile_options(serialization_integration_tests PRIVATE -fsanitize=address)
    target_link_options(serialization_integration_tests PRIVATE -fsanitize=address)
endif ()
####################

include(GNUInstallDirs)

install(TARGETS topmodelbmi
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
