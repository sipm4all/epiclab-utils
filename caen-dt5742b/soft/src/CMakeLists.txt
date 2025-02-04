### @author: Roberto Preghenella
### @email: preghenella@bo.infn.it

include_directories(${ROOT_INCLUDE_DIR})

add_executable(rwavedump rwavedump.cc rwavelib.cc)
target_link_libraries(rwavedump ${Boost_LIBRARIES} ${ROOT_LIBS} ${CAEN_LIBRARIES})
install(TARGETS rwavedump RUNTIME DESTINATION bin)

add_executable(rwaveserver rwaveserver.cc rwavelib.cc)
target_link_libraries(rwaveserver ${Boost_LIBRARIES} ${CAEN_LIBRARIES})
install(TARGETS rwaveserver RUNTIME DESTINATION bin)

