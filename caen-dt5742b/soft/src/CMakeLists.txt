### @author: Roberto Preghenella
### @email: preghenella@bo.infn.it

include_directories(${ROOT_INCLUDE_DIR})

add_executable(rwavedump rwavedump.cc)
target_link_libraries(rwavedump ${Boost_LIBRARIES} ${ROOT_LIBS} ${CAEN_LIBRARIES})
install(TARGETS rwavedump RUNTIME DESTINATION bin)

