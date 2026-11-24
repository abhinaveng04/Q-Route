add_executable(bench_mcm
    bench_mcm.cpp
)

target_link_libraries(bench_mcm PRIVATE
    qroute_pipeline
    qroute_unit3
    qroute_unit1
    qroute_common
)

target_enable_warnings(bench_mcm)
target_enable_sanitizers(bench_mcm)

add_executable(bench_traversal
    bench_traversal.cpp
)

target_link_libraries(bench_traversal PRIVATE
    qroute_pipeline
    qroute_common
)

target_enable_warnings(bench_traversal)
target_enable_sanitizers(bench_traversal)

add_executable(bench_dijkstra
    bench_dijkstra.cpp
)

target_link_libraries(bench_dijkstra PRIVATE
    qroute_pipeline
    qroute_common
)

target_enable_warnings(bench_dijkstra)
target_enable_sanitizers(bench_dijkstra)

add_executable(bench_priority_queues
    bench_priority_queues.cpp
)

target_link_libraries(bench_priority_queues PRIVATE
    qroute_pipeline
    qroute_common
)

target_enable_warnings(bench_priority_queues)
target_enable_sanitizers(bench_priority_queues)

add_executable(bench_bnb
    bench_bnb.cpp
)

target_link_libraries(bench_bnb PRIVATE
    qroute_pipeline
    qroute_common
)

target_enable_warnings(bench_bnb)
target_enable_sanitizers(bench_bnb)

