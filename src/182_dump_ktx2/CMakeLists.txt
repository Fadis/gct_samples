add_executable( dump_ktx2 dump_ktx2.cpp )
target_link_libraries( dump_ktx2
  gct
  KTX::ktx
  Threads::Threads
  Boost::program_options
  Boost::context
  Boost::system
)


