// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab

#include "osd_tracer.h"

namespace tracing {
namespace osd {

#ifdef HAVE_JAEGER

using namespace jaeger_configuration;
jaegertracing::Config jaeger_config(false, const_sampler, reporter_default_config, headers_config, baggage_config, "osd", std::vector<jaegertracing::Tag>());

tracing::Tracer tracer;

#else // !HAVE_JAEGER

tracing::Tracer tracer;

#endif

} // namespace osd
} // namespace tracing
