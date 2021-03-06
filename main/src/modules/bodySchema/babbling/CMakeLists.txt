# Copyright: (C) 2014 WYSIWYD Consortium
# Authors: Martina Zambelli
# CopyPolicy: Released under the terms of the GNU GPL v2.0.

set(PROJECTNAME babbling)
project(${PROJECTNAME})

file(GLOB sources src/*.cpp)
file(GLOB headers include/*.h)
set(docs ${PROJECTNAME}.xml)

source_group("Source Files" FILES ${sources})
source_group("Header Files" FILES ${headers})
source_group("DOC Files"    FILES ${docs})

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include ${wrdac_INCLUDE_DIRS} ${YARP_INCLUDE_DIRS} ${OTL_INCLUDE_DIRS} ${OpenCV_INCLUDE_DIRS})
add_executable(${PROJECTNAME} ${headers} ${sources} ${docs})
target_link_libraries(${PROJECTNAME} wrdac ${YARP_LIBRARIES} ${OTL_LIBRARIES} ${OpenCV_LIBRARIES})
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER bodySchema)
install(TARGETS ${PROJECTNAME} DESTINATION bin)
yarp_install(FILES ${doc_files} DESTINATION ${WYSIWYD_MODULES_INSTALL_DIR})
