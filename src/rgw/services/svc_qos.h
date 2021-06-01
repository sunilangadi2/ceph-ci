// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab ft=cpp

#pragma once

#include "rgw/rgw_service.h"


class RGWSI_QoS : public RGWServiceInstance
{
  RGWSI_Zone *zone_svc{nullptr};

public:
  RGWSI_QoS(CephContext *cct): RGWServiceInstance(cct) {}

  void init(RGWSI_Zone *_zone_svc) {
    zone_svc = _zone_svc;
  }

  const RGWQoSInfo& get_bucket_qos() const;
  const RGWQoSInfo& get_user_qos() const;
  const RGWQoSInfo& get_anon_qos() const;
};
