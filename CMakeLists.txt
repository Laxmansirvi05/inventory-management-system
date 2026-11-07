cmake_minimum_required(VERSION 3.16)
project(ims CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()
add_compile_options(-Wall -Wextra)

# In-memory core: data structures and algorithms (no database, no network).
add_library(imscore STATIC
    src/inventory.cpp
    src/sorting.cpp
    src/orders.cpp
    src/restock.cpp
    src/undo.cpp)
target_include_directories(imscore PUBLIC include)

# Console tests for the core (Phases 2-3). No database needed.
add_executable(core_tests tests/core_tests.cpp)
target_link_libraries(core_tests imscore)

# Database layer on the MySQL C API (Phase 4).
find_path(MYSQL_INCLUDE_DIR mysql/mysql.h
    HINTS /opt/homebrew/opt/mysql/include /opt/homebrew/include /usr/local/opt/mysql/include
    PATH_SUFFIXES mariadb REQUIRED)
find_library(MYSQL_CLIENT_LIB NAMES mysqlclient mariadb
    HINTS /opt/homebrew/opt/mysql/lib /opt/homebrew/lib /usr/local/opt/mysql/lib REQUIRED)

add_library(imsdb STATIC
    src/database.cpp
    src/sqllog.cpp
    src/repository.cpp)
target_include_directories(imsdb PUBLIC include third_party)
target_include_directories(imsdb SYSTEM PRIVATE ${MYSQL_INCLUDE_DIR})
target_link_libraries(imsdb PUBLIC imscore ${MYSQL_CLIENT_LIB})

# Needs a running MySQL: scripts/load_db.sh ims_test && DB_NAME=ims_test build/db_tests
add_executable(db_tests tests/db_tests.cpp)
target_link_libraries(db_tests imsdb)

# The web server (Phase 5): REST API + static files.
if(APPLE)
    list(APPEND CMAKE_PREFIX_PATH /opt/homebrew/opt/openssl@3)   # Homebrew keeps OpenSSL outside the default paths
endif()
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

add_executable(ims
    src/main.cpp
    src/server.cpp
    src/auth.cpp
    src/routes_auth.cpp
    src/routes_catalog.cpp
    src/routes_stock.cpp
    src/routes_reports.cpp)
target_include_directories(ims SYSTEM PRIVATE third_party)
target_link_libraries(ims imsdb OpenSSL::Crypto Threads::Threads)

enable_testing()
add_test(NAME core COMMAND core_tests)
