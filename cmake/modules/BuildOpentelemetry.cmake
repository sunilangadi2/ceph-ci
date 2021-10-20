function(build_opentelemetry)
  set(opentelemetry_SOURCE_DIR "${CMAKE_SOURCE_DIR}/src/jaegertracing/opentelemetry-cpp")
  set(opentelemetry_BINARY_DIR "${CMAKE_BINARY_DIR}/external/opentelemetry-cpp")

  set(opentelemetry_CMAKE_ARGS -DCMAKE_POSITION_INDEPENDENT_CODE=ON
                              -DWITH_JAEGER=ON
                              -DBUILD_TESTING=OFF
                              -DWITH_EXAMPLES=OFF
                              -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/external
                              -DCMAKE_INSTALL_RPATH=${CMAKE_BINARY_DIR}/external
                              -DCMAKE_INSTALL_RPATH_USE_LINK_PATH=TRUE
                              -DCMAKE_INSTALL_LIBDIR=${CMAKE_BINARY_DIR}/external/lib
                              -DCMAKE_PREFIX_PATH=${CMAKE_BINARY_DIR}/external)
  
  set(opentelemetry_cpp_targets opentelemetry_trace opentelemetry_exporter_jaeger_trace)

  if(CMAKE_MAKE_PROGRAM MATCHES "make")
    # try to inherit command line arguments passed by parent "make" job
    set(make_cmd $(MAKE) ${opentelemetry_cpp_targets})
  else()
    set(make_cmd ${CMAKE_COMMAND} --build <BINARY_DIR> --target ${opentelemetry_cpp_targets})
  endif()
  set(install_cmd DESTDIR= ${CMAKE_MAKE_PROGRAM} install)

  include(ExternalProject)
  ExternalProject_Add(opentelemetry-cpp
    GIT_REPOSITORY https://github.com/ideepika/opentelemetry-cpp.git
    GIT_TAG wip-ceph
    GIT_SHALLOW 1
    UPDATE_COMMAND ""
    INSTALL_DIR "external"
    PREFIX "external/opentelemetry-cpp"
    CMAKE_ARGS ${opentelemetry_CMAKE_ARGS}
    #BUILD_IN_SOURCE 1
    BUILD_COMMAND ${make_cmd}
    INSTALL_COMMAND ${install_cmd}
    BUILD_BYPRODUCTS ${CMAKE_BINARY_DIR}/external/lib/libopentelemetry.so
    LOG_BUILD OFF)
  
# will do all linking and path setting
  add_library(opentelemetry::libopentelemetry SHARED IMPORTED)
  add_dependencies(opentelemetry::libopentelemetry opentelemetry-cpp)
  set(_includepath "${CMAKE_BINARY_DIR}/external/include")
  file(MAKE_DIRECTORY "${_includepath}")

  set_target_properties(opentelemetry::libopentelemetry PROPERTIES
    INTERFACE_LINK_LIBRARIES "${opentelemetry_cpp_targets}"
    IMPORTED_LINK_INTERFACE_LANGUAGES "CXX"
    IMPORTED_LOCATION "${_libpath}"
    INTERFACE_INCLUDE_DIRECTORIES "${_includepath}/opentelemetry-cpp/exporters/jaeger/include")
endfunction()
