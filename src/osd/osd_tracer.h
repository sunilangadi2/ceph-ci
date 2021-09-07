// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab
#pragma once

#include "common/tracer.h"


#ifdef HAVE_JAEGER
 extern jaegertracing::Config jaeger_config;
 extern thread_local tracing::Tracer tracer;
#else
 extern tracing::Tracer tracer;
#endif

