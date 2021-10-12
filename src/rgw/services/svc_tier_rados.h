// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab ft=cpp

/*
 * Ceph - scalable distributed file system
 *
 * Copyright (C) 2019 Red Hat, Inc.
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software
 * Foundation. See file COPYING.
 *
 */


#pragma once

#include <iomanip>

#include "rgw/rgw_service.h"

#include "svc_rados.h"

/**
 * A filter to a) test whether an object name is a multipart meta
 * object, and b) filter out just the key used to determine the bucket
 * index shard.
 *
 * Objects for multipart meta have names adorned with an upload id and
 * other elements -- specifically a ".", MULTIPART_UPLOAD_ID_PREFIX,
 * unique id, and MP_META_SUFFIX. This filter will return true when
 * the name provided is such. It will also extract the key used for
 * bucket index shard calculation from the adorned name.
 */
class MultipartMetaFilter : public RGWAccessListFilter {
public:
  MultipartMetaFilter() {}

  virtual ~MultipartMetaFilter() override;

  /**
   * @param name [in] The object name as it appears in the bucket index.
   * @param key [out] An output parameter that will contain the bucket
   *        index key if this entry is in the form of a multipart meta object.
   * @return true if the name provided is in the form of a multipart meta
   *         object, false otherwise
   */
  bool filter(const std::string& name, std::string& key) override;
};

class RGWSI_Tier_RADOS : public RGWServiceInstance
{
  RGWSI_Zone *zone_svc{nullptr};

public:
  RGWSI_Tier_RADOS(CephContext *cct): RGWServiceInstance(cct) {}

  void init(RGWSI_Zone *_zone_svc) {
    zone_svc = _zone_svc;
  }

  static inline bool raw_obj_to_obj(const rgw_bucket& bucket, const rgw_raw_obj& raw_obj, rgw_obj *obj) {
    ssize_t pos = raw_obj.oid.find('_', bucket.marker.length());
    if (pos < 0) {
      return false;
    }

    if (!rgw_obj_key::parse_raw_oid(raw_obj.oid.substr(pos + 1), &obj->key)) {
      return false;
    }
    obj->bucket = bucket;

    return true;
  }
};

