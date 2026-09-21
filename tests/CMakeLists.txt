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