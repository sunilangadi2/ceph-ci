// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab
#pragma once

#include "common/tracer.h"

namespace tracing {
namespace osd {

#ifdef HAVE_JAEGER
extern const jaegertracing::Config jaeger_config;
#endif

extern tracing::Tracer tracer;

} // namespace osd
} // namespace tracing
