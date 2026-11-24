add_library(qroute_common STATIC
    common/safe_arithmetic.cpp
    common/file_io.cpp
    common/deterministic_time.cpp
)

target_include_directories(qroute_common PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_enable_warnings(qroute_common)
target_enable_sanitizers(qroute_common)

add_library(qroute_unit1 STATIC
    unit1_ast/lexer.cpp
    unit1_ast/threaded_ast.cpp
    unit1_ast/qasm_parser.cpp
)

target_include_directories(qroute_unit1 PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(qroute_unit1 PUBLIC
    qroute_common
)

target_enable_warnings(qroute_unit1)
target_enable_sanitizers(qroute_unit1)

add_library(qroute_unit2 STATIC
    unit2_graph/physical_coupling_graph.cpp
    unit2_graph/placement_map.cpp
    unit2_graph/swap_synthesizer.cpp
)

target_include_directories(qroute_unit2 PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(qroute_unit2 PUBLIC
    qroute_common
    qroute_unit1
)

target_enable_warnings(qroute_unit2)
target_enable_sanitizers(qroute_unit2)

add_library(qroute_unit3 STATIC
    unit3_dp/matrix_chain_optimizer.cpp
)

target_include_directories(qroute_unit3 PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(qroute_unit3 PUBLIC
    qroute_common
    qroute_unit1
)

target_enable_warnings(qroute_unit3)
target_enable_sanitizers(qroute_unit3)

add_library(qroute_unit4 STATIC
    unit4_bnb/lookahead_graph.cpp
    unit4_bnb/qubit_placement_bnb.cpp
)

target_include_directories(qroute_unit4 PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(qroute_unit4 PUBLIC
    qroute_common
    qroute_unit1
    qroute_unit2
)

target_enable_warnings(qroute_unit4)
target_enable_sanitizers(qroute_unit4)

add_library(qroute_unit5 STATIC
    unit5_heap/binary_heap_baseline.cpp
    unit5_heap/telemetry_config.cpp
    unit5_heap/telemetry_broker.cpp
)

target_include_directories(qroute_unit5 PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(qroute_unit5 PUBLIC
    qroute_common
)

target_enable_warnings(qroute_unit5)
target_enable_sanitizers(qroute_unit5)

add_library(qroute_pipeline STATIC
    pipeline/execution_engine.cpp
    pipeline/report_writer.cpp
    pipeline/benchmark_suite.cpp
)

target_include_directories(qroute_pipeline PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(qroute_pipeline PUBLIC
    qroute_common
    qroute_unit1
    qroute_unit2
    qroute_unit3
    qroute_unit4
    qroute_unit5
)

target_enable_warnings(qroute_pipeline)
target_enable_sanitizers(qroute_pipeline)


