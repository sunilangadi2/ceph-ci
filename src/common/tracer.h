// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab

#ifndef TRACER_H
#define TRACER_H

#ifdef HAVE_JAEGER

#define SIGNED_RIGHT_SHIFT_IS 1
#define ARITHMETIC_RIGHT_SHIFT 1
#include <jaegertracing/Tracer.h>

typedef std::unique_ptr<opentracing::Span> jspan;

namespace tracing {

class Tracer {
 private:
  const static std::shared_ptr<opentracing::Tracer> noop_tracer;
  std::shared_ptr<opentracing::Tracer> open_tracer;

 public:
  Tracer(jaegertracing::Config& conf);
  Tracer(opentracing::string_view service_name);
  bool is_enabled() const;
  // creates and returns a new span with `trace_name`
  // this span represents a trace, since it has no parent.
  jspan start_trace(opentracing::string_view trace_name);
  // creates and returns a new span with `span_name` which parent span is `parent_span'
  jspan start_span(opentracing::string_view span_name, jspan& parent_span);
};


} // namespace tracing

extern thread_local tracing::Tracer tracer;

namespace jaeger_configuration {

extern jaegertracing::Config jaeger_rgw_config;
extern jaegertracing::Config jaeger_osd_config;

}

#else  // !HAVE_JAEGER
class Value {
 public:
  template <typename T> Value(T val) {}
};

struct span_stub {
  template <typename T>
  void SetTag(std::string_view key, const T& value) const noexcept {}
  void Log(std::initializer_list<std::pair<std::string_view, Value>> fields) {}
};

class jspan {
  span_stub span;
 public:
  span_stub& operator*() { return span; }
  const span_stub& operator*() const { return span; }

  span_stub* operator->() { return &span; }
  const span_stub* operator->() const { return &span; }

  operator bool() const { return false; }
};

namespace tracing {

struct Tracer {
  bool is_enabled() const { return false; }
  jspan start_trace(std::string_view) { return {}; }
  jspan start_span(std::string_view, const jspan&) { return {}; }
};
}

extern tracing::Tracer tracer;

#endif // !HAVE_JAEGER


namespace tracing {

const auto OP = "op";
const auto BUCKET_NAME = "bucket_name";
const auto USER_ID = "user_id";
const auto OBJECT_NAME = "object_name";
const auto RETURN = "return";
const auto UPLOAD_ID = "upload_id";
const auto TYPE = "type";
const auto REQUEST = "request";
}

#endif // TRACER_H
