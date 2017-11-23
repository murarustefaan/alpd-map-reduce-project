cmake_minimum_required(VERSION 3.8)
project(MapReduce_V2)

set(CMAKE_C_STANDARD 99)

set(MPIEXEC "mpirun")

set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g")

message(STATUS "${MPI_C_LIBRARIES}")

find_package(MPI REQUIRED)
if (NOT MPI_FOUND)
    message(FATAL_ERROR "MPI NOT FOUND!")
endif()

include_directories(${MPI_INCLUDE_PATH})

set(SOURCE_FILES main.c src/FileOperations.c defs/FileOperations.h src/Utils.c defs/Utils.h defs/DirectoryFiles.h defs/ErrorHandling.h src/ErrorHandling.c defs/MapReduceOperation.h)
add_executable(MapReduce_V2 ${SOURCE_FILES})

target_link_libraries(MapReduce_V2 ${MPI_LIBRARIES})
if (MPI_COMPILE_FLAGS)
    set_target_properties(MapReduce_V2 PROPERTIES COMPILE_FLAGS "${MPI_COMPILE_FLAGS}")
endif()

if (MPI_LINK_FLAGS)
    set_target_properties(MapReduce_V2 PROPERTIES LINK_FLAGS "${MPI_LINK_FLAGS}")
endif()