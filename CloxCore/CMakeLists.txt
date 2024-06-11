file(GLOB_RECURSE SOURCES "${PROJECT_SOURCE_DIR}/CloxCore/src/**.c")
add_executable(clox "${SOURCES}")
target_include_directories(clox PUBLIC "${PROJECT_SOURCE_DIR}/CloxCore/include")

install(TARGETS clox DESTINATION bin)
