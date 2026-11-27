add_executable(freeframe_tests
  test_main.cpp
  test_format.cpp
  test_gpu_backend.cpp
  test_result.cpp
)
target_link_libraries(freeframe_tests PRIVATE FreeFrame::core doctest::doctest)
freeframe_set_warnings(freeframe_tests)
add_test(NAME freeframe_tests COMMAND freeframe_tests)
