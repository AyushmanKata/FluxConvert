# Add test framework
add_subdirectory(unit)
add_subdirectory(integration)

# Test fixtures directory
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/fixtures
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
