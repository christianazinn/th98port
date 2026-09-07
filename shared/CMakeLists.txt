add_library(pc98_numeric INTERFACE)
target_include_directories(pc98_numeric INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(pc98_numeric INTERFACE cxx_std_23)
