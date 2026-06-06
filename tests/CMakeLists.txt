add_executable(ricc_tests
    test_lexer.cpp
    test_parser.cpp
    test_semantic.cpp
    test_optimizer.cpp
)

target_link_libraries(ricc_tests
    PRIVATE
        ricc_lib
        Catch2::Catch2WithMain
)

target_include_directories(ricc_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/compiler/include
)

include(CTest)
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(ricc_tests)
