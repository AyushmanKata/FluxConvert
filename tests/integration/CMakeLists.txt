# Integration tests for image conversion

# Fixture generator
add_executable(generate_fixtures
    ../fixtures/generate_fixtures.cpp
)
target_link_libraries(generate_fixtures PRIVATE fluxconvert-core)

# Generate fixtures at build time
add_custom_target(test_fixtures
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/../fixtures
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/../output
    COMMAND $<TARGET_FILE:generate_fixtures>
    DEPENDS generate_fixtures
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Generating test fixtures..."
)

# Image conversion integration tests
add_executable(test_image_conversion
    test_image_conversion.cpp
)
target_link_libraries(test_image_conversion PRIVATE fluxconvert-core)
add_dependencies(test_image_conversion test_fixtures)

add_test(NAME ImageConversion COMMAND test_image_conversion
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

# PDF generation integration tests
add_executable(test_pdf_generation
    test_pdf_generation.cpp
)
target_link_libraries(test_pdf_generation PRIVATE fluxconvert-core)
add_dependencies(test_pdf_generation test_fixtures)

add_test(NAME PdfGeneration COMMAND test_pdf_generation
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

# PDF rendering integration tests
add_executable(test_pdf_rendering
    test_pdf_rendering.cpp
)
target_link_libraries(test_pdf_rendering PRIVATE fluxconvert-core)
add_dependencies(test_pdf_rendering test_fixtures)

add_test(NAME PdfRendering COMMAND test_pdf_rendering
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Batch execution integration tests
add_executable(test_batch_execution
    test_batch_execution.cpp
)
target_link_libraries(test_batch_execution PRIVATE fluxconvert-core)
add_dependencies(test_batch_execution test_fixtures)

add_test(NAME BatchExecution COMMAND test_batch_execution
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
