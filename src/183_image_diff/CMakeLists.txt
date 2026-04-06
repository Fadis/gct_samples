add_executable( image_diff image_diff.cpp )
target_link_libraries( image_diff
  gct
  Threads::Threads
  Boost::program_options
  Boost::context
  Boost::system
)


