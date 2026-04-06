add_executable( dump_ktx1 dump_ktx1.cpp )
target_link_libraries( dump_ktx1
  gct
  KTX::ktx
  Threads::Threads
  Boost::program_options
  Boost::context
  Boost::system
)


