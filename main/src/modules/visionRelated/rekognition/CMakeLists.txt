# Copyright: (C) 2014 WYSIWYD Consortium
# Authors: Maxime Petit and Tobias Fischer
# CopyPolicy: Released under the terms of the GNU GPL v2.0.

set(PROJECTNAME rekognition)
project(${PROJECTNAME})

file(GLOB folder_source src/*.cpp src/json/*.cpp)
file(GLOB folder_header include/*.h include/json/*.h include/json/*.inl)
set(doc_files ${PROJECTNAME}.xml)

source_group("Header Files" FILES ${folder_header})
source_group("Source Files" FILES ${folder_source})
source_group("DOC Files"    FILES ${doc_files})

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include ${YARP_INCLUDE_DIRS} ${CURL_INCLUDE_DIRS})

add_executable(${PROJECTNAME} ${folder_header} ${folder_source} ${doc_files})
target_link_libraries(${PROJECTNAME} wrdac ${YARP_LIBRARIES} ${CURL_LIBRARIES})
set_target_properties(${PROJECTNAME} PROPERTIES FOLDER visionRelated)
install(TARGETS ${PROJECTNAME} DESTINATION bin)
yarp_install(FILES ${doc_files} DESTINATION ${WYSIWYD_MODULES_INSTALL_DIR})
