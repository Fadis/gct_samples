add_executable( onesweep
  gct.cpp
)
target_compile_definitions( onesweep PRIVATE -DCMAKE_CURRENT_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}" )
target_compile_definitions( onesweep PRIVATE -DCMAKE_CURRENT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}" )

add_shader( onesweep onesweep32/histgram.comp )
add_shader( onesweep onesweep32/histgram_sum.comp )
add_shader( onesweep onesweep32/local_sum.comp )
add_shader( onesweep onesweep32/global_sum.comp )
add_shader( onesweep onesweep32/sort.comp )
add_shader( onesweep onesweep32/small_sort.comp )

