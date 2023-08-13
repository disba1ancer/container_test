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
    avl_tree.h
    avl_tree_node.h
    bs_tree.h
    bs_tree_node.h
    hash_table.h
    list.h
    list_node.h
    main.cpp
    node.h
    oc_queue.h
    slist.h
    slist_node.h
    util.h
)

add_executable(malloc_test
    allocator.h
    avl_tree.h
    avl_tree_node.h
    bs_tree.h
    bs_tree_node.h
    hash_table.h
    list.h
    list_node.h
    mymalloc.cpp
    node.h
    oc_queue.h
    slist.h
    slist_node.h
    util.h
)
