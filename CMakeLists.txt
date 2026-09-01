cmake_minimum_required(VERSION 3.16)
project(MxTree CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Every directory is a complete, self-contained variant: its own copy of the
# Berkeley libgist core plus the tree built on top of it. They are near
# duplicates on purpose, since each one changes the internals rather than
# parameterising them, so there is one executable per directory and no shared
# library between them.
set(MXTREE_VARIANTS
    MTree
    MTree_int
    MTree_str
    MxTree
    MxTree_int
    MxTree_str
    MxTree_SLIM
    MxTree_SLIM_str
    MxTree_SPLIT
    MxTree_SPLIT_str)

foreach(variant ${MXTREE_VARIANTS})
  if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${variant}/MxTree")
    set(family MxTree)
  else()
    set(family MTree)
  endif()

  file(GLOB variant_sources
       "${CMAKE_CURRENT_SOURCE_DIR}/${variant}/Gist/*.cpp"
       "${CMAKE_CURRENT_SOURCE_DIR}/${variant}/${family}/*.cpp")

  add_executable(${variant} ${variant_sources})

  target_include_directories(${variant} PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}/compat"
      "${CMAKE_CURRENT_SOURCE_DIR}/${variant}/Gist"
      "${CMAKE_CURRENT_SOURCE_DIR}/${variant}/${family}")

  # PRINTING_OBJECTS is one of the switches the original Bologna makefile set;
  # its UNIX companion is gone now that there is only one platform.
  # _FILE_OFFSET_BITS keeps off_t 64 bit on 32 bit hosts, which the storage
  # layer assumes when it seeks by page number.
  target_compile_definitions(${variant} PRIVATE
      PRINTING_OBJECTS
      _FILE_OFFSET_BITS=64)
endforeach()
