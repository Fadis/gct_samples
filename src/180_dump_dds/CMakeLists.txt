add_executable( dump_dds dump_dds.cpp )
target_link_libraries( dump_dds
  gct
  Threads::Threads
  Boost::program_options
  Boost::context
  Boost::system
)


