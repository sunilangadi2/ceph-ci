// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab

#include <string>
#include "rgw_tracer.h"

namespace tracing {
namespace rgw {

#ifdef HAVE_JAEGER
 using namespace jaeger_configuration;
 jaegertracing::Config jaeger_config(false, const_sampler, reporter_default_config, headers_config, baggage_config, "rgw", std::vector<jaegertracing::Tag>());
 thread_local tracing::Tracer tracer(tracing::rgw::jaeger_config);
#else // !HAVE_JAEGER
 tracing::Tracer tracer;
#endif

} // namespace rgw
} // namespace tracing
