cmake_minimum_required(VERSION 3.5)

project(container_test LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(COMPILER_COMPAT ${CMAKE_CXX_COMPILER_ID})
if (${COMPILER_COMPAT} MATCHES "Clang")
    set(COMPILER_COMPAT ${CMAKE_CXX_COMPILER_FRONTEND_VARIANT})
endif()

if (${COMPILER_COMPAT} MATCHES "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -pedantic-errors")
elseif(${COMPILER_COMPAT} MATCHES "MSVC")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
endif()

add_executable(container_test
    avl_tree.hpp
    avl_tree_node.hpp
    bs_tree.hpp
    bs_tree_node.hpp
    hash_table.hpp
    list.hpp
    list_node.hpp
    main.cpp
    node.hpp
    oc_queue.hpp
    slist.hpp
    slist_node.hpp
    util.hpp
)

add_executable(malloc_test
    allocator.hpp
    avl_tree.hpp
    avl_tree_node.hpp
    bs_tree.hpp
    bs_tree_node.hpp
    hash_table.hpp
    list.hpp
    list_node.hpp
    mymalloc.cpp
    node.hpp
    oc_queue.hpp
    slist.hpp
    slist_node.hpp
    util.hpp
)
