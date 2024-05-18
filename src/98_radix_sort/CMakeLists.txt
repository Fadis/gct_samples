add_executable( radix_sort
  gct.cpp
)
target_compile_definitions( radix_sort PRIVATE -DCMAKE_CURRENT_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}" )
target_compile_definitions( radix_sort PRIVATE -DCMAKE_CURRENT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}" )

add_shader( radix_sort radix_sort32/local_sum.comp )
add_shader( radix_sort radix_sort32/global_sum.comp )
add_shader( radix_sort radix_sort32/sort.comp )
add_shader( radix_sort radix_sort32/small_sort.comp )

