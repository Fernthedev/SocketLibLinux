set(ANDROID_LD lld) # Android NDK buggy?

# include some defines automatically made by qpm
include(qpm_defines.cmake)

# Enable link time optimization
# In my experience, this can be highly unstable but it nets a huge size optimization and likely performance
# However, the instability was seen using Android.mk/ndk-build builds. With Ninja + CMake, this problem seems to have been solved.
# As always, test thoroughly
# - Fern
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)

cmake_minimum_required(VERSION 3.21)
project(${COMPILE_ID})

# c++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# define that stores the actual source directory
set(SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

# compile options used
add_compile_options(-frtti -fexceptions)
add_compile_options(-O3)

# compile definitions used
add_compile_definitions(VERSION=\"${MOD_VERSION}\")

# add_compile_definitions(ID=\"${MOD_ID}\")
add_compile_definitions(MOD_ID=\"${MOD_ID}\")
add_compile_definitions(DEBUGB=1) # disable debug logs

if(DEFINED ANDROID)
  if(NOT DEFINED NOT_QUEST)
    message("Building for Quest")
    add_compile_definitions(QUEST)
    
    if (CMAKE_BUILD_TYPE EQUAL "DEBUG")
      add_compile_definitions(QUEST_TEST)
    endif()
  else()
    message("Building for Quest without Quest defines")
  endif()
endif()

if(DEFINED PROFILE)
  add_compile_options(-pg -lprofiler)
  add_compile_definitions(PROFILE) # disable debug logs
endif()

add_compile_definitions(FMT_HEADER_ONLY)

# recursively get all src files
RECURSE_FILES(cpp_file_list ${SOURCE_DIR}/*.cpp)
RECURSE_FILES(c_file_list ${SOURCE_DIR}/*.c)

if(DEFINED BUILD_TEST)
  message("Building tests")
  RECURSE_FILES(test_cpp_file_list test/src/*.cpp)

  SET(cpp_file_list ${cpp_file_list} ${test_cpp_file_list})
endif()

# add all src files to compile
add_library(
  ${COMPILE_ID}
  SHARED
  ${cpp_file_list}
  ${c_file_list}
)

target_include_directories(${COMPILE_ID} PRIVATE .)

target_include_directories(${COMPILE_ID} PRIVATE test/include)

# add src dir as include dir
target_include_directories(${COMPILE_ID} PRIVATE ${SOURCE_DIR})

# add include dir as include dir
target_include_directories(${COMPILE_ID} PRIVATE ${INCLUDE_DIR})

# add shared dir as include dir
target_include_directories(${COMPILE_ID} PUBLIC ${SHARED_DIR})

# codegen includes
target_include_directories(${COMPILE_ID} PRIVATE ${EXTERN_DIR}/includes/${CODEGEN_ID}/include)

target_link_libraries(${COMPILE_ID} PRIVATE -llog)

# add extern stuff like libs and other includes
include(extern.cmake)

add_custom_command(TARGET ${COMPILE_ID} POST_BUILD
  COMMAND ${CMAKE_STRIP} -d --strip-all
  "lib${COMPILE_ID}.so" -o "stripped_lib${COMPILE_ID}.so"
  COMMENT "Strip debug symbols done on final binary.")

add_custom_command(TARGET ${COMPILE_ID} POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory debug
  COMMENT "Rename the lib to debug_ since it has debug symbols"
)

add_custom_command(TARGET ${COMPILE_ID} POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E rename lib${COMPILE_ID}.so debug/lib${COMPILE_ID}.so
  COMMENT "Rename the lib to debug_ since it has debug symbols"
)

add_custom_command(TARGET ${COMPILE_ID} POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E rename stripped_lib${COMPILE_ID}.so lib${COMPILE_ID}.so
  COMMENT "Rename the stripped lib to regular"
)