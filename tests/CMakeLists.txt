add_executable(freeframe_tests
  test_main.cpp
  test_format.cpp
  test_gpu_backend.cpp
  test_result.cpp
)
if(WIN32)
  target_sources(freeframe_tests PRIVATE test_d3d11.cpp)
  target_compile_definitions(freeframe_tests PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()
target_link_libraries(freeframe_tests PRIVATE FreeFrame::core doctest::doctest)
freeframe_set_warnings(freeframe_tests)
add_test(NAME freeframe_tests COMMAND freeframe_tests)
