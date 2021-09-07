// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab

#include "osd_tracer.h"


#ifdef HAVE_JAEGER

namespace tracing {
namespace osd {

using namespace jaeger_configuration;
jaegertracing::Config jaeger_config(false, const_sampler, reporter_default_config, headers_config, baggage_config, "osd", std::vector<jaegertracing::Tag>());

} // namespace osd
} // namespace tracing

thread_local tracing::Tracer tracer(tracing::osd::jaeger_config);

#else // !HAVE_JAEGER

tracing::Tracer tracer;

#endif

