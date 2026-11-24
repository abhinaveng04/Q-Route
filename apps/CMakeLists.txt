add_executable(qroute
    qroute_main.cpp
)

target_link_libraries(qroute PRIVATE
    qroute_pipeline
    qroute_unit5
    qroute_unit4
    qroute_unit3
    qroute_unit2
    qroute_unit1
    qroute_common
)

target_enable_warnings(qroute)
target_enable_sanitizers(qroute)