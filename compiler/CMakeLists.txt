set(COMPILER_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

set(COMPILER_SOURCES
    src/diagnostics/diagnostics.cpp
    src/lexer/lexer.cpp
    src/parser/parser.cpp
    src/semantic/semantic_analyzer.cpp
    src/ir/ir_builder.cpp
    src/optimizer/passes.cpp
    src/codegen/codegen.cpp
    src/driver/driver.cpp
)

add_library(ricc_lib STATIC ${COMPILER_SOURCES})

target_include_directories(ricc_lib
    PUBLIC  ${COMPILER_INCLUDE_DIR}
    PRIVATE ${COMPILER_INCLUDE_DIR}
)

target_compile_features(ricc_lib PUBLIC cxx_std_23)

add_executable(ricc src/main.cpp)
target_link_libraries(ricc PRIVATE ricc_lib)
target_include_directories(ricc PRIVATE ${COMPILER_INCLUDE_DIR})

install(TARGETS ricc DESTINATION bin)
