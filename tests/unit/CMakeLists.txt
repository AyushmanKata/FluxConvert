# Unit tests
add_executable(test_format_detection
  test_format_detection.cpp
)

target_link_libraries(test_format_detection
  PRIVATE fluxconvert-core
)

add_test(NAME FormatDetection COMMAND test_format_detection)

# Add more unit tests
add_executable(test_registry
  test_registry.cpp
)

target_link_libraries(test_registry
  PRIVATE fluxconvert-core
)

add_test(NAME Registry COMMAND test_registry)
