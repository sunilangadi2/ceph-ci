// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab ft=cpp

#include "svc_qos.h"
#include "svc_zone.h"

#include "rgw/rgw_zone.h"

const RGWQoSInfo& RGWSI_QoS::get_bucket_qos() const
{
  return zone_svc->get_current_period().get_config().bucket_qos;
}

const RGWQoSInfo& RGWSI_QoS::get_user_qos() const
{
  return zone_svc->get_current_period().get_config().user_qos;
}

const RGWQoSInfo& RGWSI_QoS::get_anon_qos() const
{
  return zone_svc->get_current_period().get_config().anon_qos;
}