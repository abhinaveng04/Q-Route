add_executable(test_common_safe_arithmetic
    test_common_safe_arithmetic.cpp
)

target_link_libraries(test_common_safe_arithmetic PRIVATE
    qroute_common
)

target_enable_warnings(test_common_safe_arithmetic)
target_enable_sanitizers(test_common_safe_arithmetic)

add_test(NAME common_safe_arithmetic
    COMMAND test_common_safe_arithmetic
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_executable(test_unit1_ast
    test_unit1_ast.cpp
)

target_link_libraries(test_unit1_ast PRIVATE
    qroute_unit1
    qroute_common
)

target_enable_warnings(test_unit1_ast)
target_enable_sanitizers(test_unit1_ast)

add_test(NAME unit1_ast
    COMMAND test_unit1_ast
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_executable(test_unit2_graph
    test_unit2_graph.cpp
)

target_link_libraries(test_unit2_graph PRIVATE
    qroute_unit2
    qroute_unit1
    qroute_common
)

target_enable_warnings(test_unit2_graph)
target_enable_sanitizers(test_unit2_graph)

add_test(NAME unit2_graph
    COMMAND test_unit2_graph
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_executable(test_unit3_dp
    test_unit3_dp.cpp
)

target_link_libraries(test_unit3_dp PRIVATE
    qroute_unit3
    qroute_unit1
    qroute_common
)

target_enable_warnings(test_unit3_dp)
target_enable_sanitizers(test_unit3_dp)

add_test(NAME unit3_dp
    COMMAND test_unit3_dp
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_executable(test_unit4_bnb
    test_unit4_bnb.cpp
)

target_link_libraries(test_unit4_bnb PRIVATE
    qroute_unit4
    qroute_unit2
    qroute_unit1
    qroute_common
)

target_enable_warnings(test_unit4_bnb)
target_enable_sanitizers(test_unit4_bnb)

add_test(NAME unit4_bnb
    COMMAND test_unit4_bnb
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_executable(test_unit5_heap
    test_unit5_heap.cpp
)

target_link_libraries(test_unit5_heap PRIVATE
    qroute_unit5
    qroute_common
)

target_enable_warnings(test_unit5_heap)
target_enable_sanitizers(test_unit5_heap)

add_test(NAME unit5_heap
    COMMAND test_unit5_heap
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_executable(test_pipeline_e2e
    test_pipeline_e2e.cpp
)

target_link_libraries(test_pipeline_e2e PRIVATE
    qroute_pipeline
    qroute_unit5
    qroute_unit4
    qroute_unit3
    qroute_unit2
    qroute_unit1
    qroute_common
)

target_enable_warnings(test_pipeline_e2e)
target_enable_sanitizers(test_pipeline_e2e)

add_test(NAME pipeline_e2e
    COMMAND test_pipeline_e2e
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
