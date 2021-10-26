function(target_create _target _lib)
  add_library(${_target} STATIC IMPORTED)
  set_target_properties(${_target}
      PROPERTIES IMPORTED_LOCATION "${opentelemetry_BINARY_DIR}/${_lib}")
endfunction()

function(build_opentelemetry)
  set(opentelemetry_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/opentelemetry-cpp")
  set(opentelemetry_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/opentelemetry-cpp")
  set(opentelemetry_cpp_targets opentelemetry_trace opentelemetry_exporter_jaeger_trace)
  set(opentelemetry_CMAKE_ARGS -DCMAKE_POSITION_INDEPENDENT_CODE=ON
                               -DWITH_JAEGER=ON
                               -DBUILD_TESTING=OFF
                               -DWITH_EXAMPLES=OFF
                               -DCMAKE_INSTALL_PREFIX=${opentelemetry_BINARY_DIR}
                               -DCMAKE_INSTALL_RPATH=${opentelemetry_BINARY_DIR})
  set(opentelemetry_libs
      ${opentelemetry_BINARY_DIR}/sdk/src/trace/libopentelemetry_trace.a
      ${opentelemetry_BINARY_DIR}/sdk/src/resource/libopentelemetry_resources.a
      ${opentelemetry_BINARY_DIR}/sdk/src/common/libopentelemetry_common.a
      ${opentelemetry_BINARY_DIR}/exporters/jaeger/libopentelemetry_exporter_jaeger_trace.a
      ${opentelemetry_BINARY_DIR}/ext/src/http/client/curl/libhttp_client_curl.a
      ${CURL_LIBRARIES}
  )

  # TODO: add target based propogation
  include_directories(SYSTEM ${opentelemetry_BINARY_DIR}/include)
  set(opentelemetry_deps opentelemetry_trace opentelemetry_resources opentelemetry_common
                         opentelemetry_exporter_jaeger_trace http_client_curl
			 ${CURL_LIBRARIES})

  if(CMAKE_MAKE_PROGRAM MATCHES "make")
    # try to inherit command line arguments passed by parent "make" job
    set(make_cmd $(MAKE) ${opentelemetry_cpp_targets})
  else()
    set(make_cmd ${CMAKE_COMMAND} --build <BINARY_DIR> --target
                 ${opentelemetry_cpp_targets})
  endif()
  set(install_cmd DESTDIR= ${CMAKE_MAKE_PROGRAM} install)

  include(ExternalProject)
  ExternalProject_Add(
    opentelemetry-cpp
    GIT_REPOSITORY https://github.com/open-telemetry/opentelemetry-cpp.git
    GIT_TAG main
    GIT_SHALLOW 1
    SOURCE_DIR ${opentelemetry_SOURCE_DIR}
    PREFIX "opentelemetry-cpp"
    CMAKE_ARGS ${opentelemetry_CMAKE_ARGS}
    BUILD_COMMAND ${make_cmd}
    BINARY_DIR ${opentelemetry_BINARY_DIR}
    INSTALL_COMMAND ${install_cmd}
    BUILD_BYPRODUCTS ${opentelemetry_libs}
    LOG_BUILD OFF)

  # CMake doesn't allow to add a list of libraries to the import property, hence
  # we create individual targets and link their libraries which finally
  # interfaces to opentelemetry target
  target_create("opentelemetry_trace" "sdk/src/trace/libopentelemetry_trace.a")
  target_create("opentelemetry_resources" "sdk/src/resource/libopentelemetry_resources.a")
  target_create("opentelemetry_common" "sdk/src/common/libopentelemetry_common.a")
  target_create("opentelemetry_exporter_jaeger_trace" "exporters/jaeger/libopentelemetry_exporter_jaeger_trace.a")
  target_create("http_client_curl" "ext/src/http/client/curl/libhttp_client_curl.a")

  # will do all linking and path setting fake include path for
  # interface_include_directories since this happens at build time
  add_library(opentelemetry::libopentelemetry INTERFACE IMPORTED)
  add_dependencies(opentelemetry::libopentelemetry opentelemetry-cpp)
  set_target_properties( opentelemetry::libopentelemetry
    PROPERTIES
      INTERFACE_LINK_LIBRARIES "${opentelemetry_deps}"
      INTERFACE_INCLUDE_DIRECTORIES "${opentelemetry_include_dir}")
endfunction()
