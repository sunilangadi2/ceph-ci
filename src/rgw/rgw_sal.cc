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

#include <errno.h>
#include <stdlib.h>
#include <system_error>
#include <unistd.h>
#include <sstream>

#include "common/errno.h"

#include "rgw_sal.h"
#include "rgw_sal_rados.h"
#include "rgw_d3n_datacache.h"

#ifdef WITH_RADOSGW_DBSTORE
#include "rgw_sal_dbstore.h"
#endif

#define dout_subsys ceph_subsys_rgw

extern "C" {
extern rgw::sal::Store* newStore(void);
#ifdef WITH_RADOSGW_DBSTORE
extern rgw::sal::Store* newDBStore(CephContext *cct);
#endif
}

rgw::sal::Store* StoreManager::init_storage_provider(const DoutPrefixProvider* dpp, CephContext* cct, const std::string svc, bool use_gc_thread, bool use_lc_thread, bool quota_threads, bool run_sync_thread, bool run_reshard_thread, bool use_cache, bool use_gc)
{
  if (svc.compare("rados") == 0) {
    rgw::sal::Store* store = newStore();
    RGWRados* rados = static_cast<rgw::sal::RadosStore* >(store)->getRados();

    if ((*rados).set_use_cache(use_cache)
                .set_use_datacache(false)
                .set_use_gc(use_gc)
                .set_run_gc_thread(use_gc_thread)
                .set_run_lc_thread(use_lc_thread)
                .set_run_quota_threads(quota_threads)
                .set_run_sync_thread(run_sync_thread)
                .set_run_reshard_thread(run_reshard_thread)
                .initialize(cct, dpp) < 0) {
      delete store; store = nullptr;
    }
    return store;
  }
  else if (svc.compare("d3n") == 0) {
    rgw::sal::RadosStore *store = new rgw::sal::RadosStore();
    RGWRados* rados = new D3nRGWDataCache<RGWRados>;
    store->setRados(rados);
    rados->set_store(static_cast<rgw::sal::RadosStore* >(store));

    if ((*rados).set_use_cache(use_cache)
                .set_use_datacache(true)
                .set_run_gc_thread(use_gc_thread)
                .set_run_lc_thread(use_lc_thread)
                .set_run_quota_threads(quota_threads)
                .set_run_sync_thread(run_sync_thread)
                .set_run_reshard_thread(run_reshard_thread)
                .initialize(cct, dpp) < 0) {
      delete store; store = nullptr;
    }
    return store;
  }

  if (svc.compare("dbstore") == 0) {
#ifdef WITH_RADOSGW_DBSTORE
    rgw::sal::Store* store = newDBStore(cct);

    /* Initialize the dbstore with cct & dpp */
    DB *db = static_cast<rgw::sal::DBStore *>(store)->getDB();
    db->set_context(cct);

    /* XXX: temporary - create testid user */
    rgw_user testid_user("", "testid", "");
    std::unique_ptr<rgw::sal::User> user = store->get_user(testid_user);
    user->get_info().display_name = "M. Tester";
    user->get_info().user_email = "tester@ceph.com";
    RGWAccessKey k1("0555b35654ad1656d804", "h7GhxuBLTrlhVUyxSPUKUV8r/2EI4ngqJxD7iBdBYLhwluN30JaT3Q==");
    user->get_info().access_keys["0555b35654ad1656d804"] = k1;

    int r = user->store_user(dpp, null_yield, true);
    if (r < 0) {
      ldpp_dout(dpp, 0) << "ERROR: failed inserting testid user in dbstore error r=" << r << dendl;
    }
    return store;
#endif
  }

  return nullptr;
}

rgw::sal::Store* StoreManager::init_raw_storage_provider(const DoutPrefixProvider* dpp, CephContext* cct, const std::string svc)
{
  rgw::sal::Store* store = nullptr;
  if (svc.compare("rados") == 0) {
    store = newStore();
    RGWRados* rados = static_cast<rgw::sal::RadosStore* >(store)->getRados();

    rados->set_context(cct);

    int ret = rados->init_svc(true, dpp);
    if (ret < 0) {
      ldout(cct, 0) << "ERROR: failed to init services (ret=" << cpp_strerror(-ret) << ")" << dendl;
      delete store; store = nullptr;
      return store;
    }

    if (rados->init_rados() < 0) {
      delete store; store = nullptr;
    }
  }

  if (svc.compare("dbstore") == 0) {
#ifdef WITH_RADOSGW_DBSTORE
    store = newDBStore(cct);
#else
    store = nullptr;
#endif
  }
  return store;
}

void StoreManager::close_storage(rgw::sal::Store* store)
{
  if (!store)
    return;

  store->finalize();

  delete store;
}

namespace rgw::sal {
int Object::range_to_ofs(uint64_t obj_size, int64_t &ofs, int64_t &end)
{
  if (ofs < 0) {
    ofs += obj_size;
    if (ofs < 0)
      ofs = 0;
    end = obj_size - 1;
  } else if (end < 0) {
    end = obj_size - 1;
  }

  if (obj_size > 0) {
    if (ofs >= (off_t)obj_size) {
      return -ERANGE;
    }
    if (end >= (off_t)obj_size) {
      end = obj_size - 1;
    }
  }
  return 0;
}

std::unique_ptr<rgw::sal::Object> MultipartUpload::create_meta_obj(rgw::sal::Store *store, RGWMPObj& mp_obj)
{
  std::string oid = mp_obj.get_key();

  char buf[33];
  string tmp_obj_name;
  std::unique_ptr<rgw::sal::Object> obj;
  gen_rand_alphanumeric(store->ctx(), buf, sizeof(buf) - 1);
  std::string upload_id = MULTIPART_UPLOAD_ID_PREFIX; /* v2 upload id */
  upload_id.append(buf);

  mp_obj.init(oid, upload_id);
  tmp_obj_name = mp_obj.get_meta();

  obj = get_meta_obj();

  return obj;
}

int MultipartUpload::get_sorted_omap_part_str(int marker, std::string& p) {
    p = "part.";
    char buf[32];

    snprintf(buf, sizeof(buf), "%08d", marker);
    p.append(buf);
    return 0;
}

int MultipartUpload::list_parts(const DoutPrefixProvider *dpp, CephContext *cct,
				     int num_parts, int marker,
				     int *next_marker, bool *truncated,
				     bool assume_unsorted)
{
  map<string, bufferlist> parts_map;
  map<string, bufferlist>::iterator iter;

  std::unique_ptr<rgw::sal::Object> obj = get_meta_obj();
  obj->set_in_extra_data(true);

  bool sorted_omap = is_v2_upload_id(get_upload_id()) && !assume_unsorted;

  parts.clear();

  int ret;
  if (sorted_omap) {
    std::string p;
    (void)get_sorted_omap_part_str(marker, p);

    ret = obj->omap_get_vals(dpp, p, num_parts + 1, &parts_map,
                                 nullptr, null_yield);
  } else {
    ret = obj->omap_get_all(dpp, &parts_map, null_yield);
  }
  if (ret < 0) {
    return ret;
  }

  int i;
  int last_num = 0;

  uint32_t expected_next = marker + 1;

  for (i = 0, iter = parts_map.begin();
       (i < num_parts || !sorted_omap) && iter != parts_map.end();
       ++iter, ++i) {
    bufferlist& bl = iter->second;
    std::unique_ptr<MultipartPart> part;

    part = get_multipart_part(dpp, bl);

    if (!part) {
      return -EIO;
    }

    if (sorted_omap) {
      if (part->get_num() != expected_next) {
        /* ouch, we expected a specific part num here, but we got a
         * different one. Either a part is missing, or it could be a
         * case of mixed rgw versions working on the same upload,
         * where one gateway doesn't support correctly sorted omap
         * keys for multipart upload just assume data is unsorted.
         */
        return list_parts(dpp, cct, num_parts, marker, next_marker, truncated, true);
      }
      expected_next++;
    }
    if (sorted_omap ||
      (int)part->get_num() > marker) {
      last_num = part->get_num();
      parts[part->get_num()] = std::move(part);
    }
  }

  if (sorted_omap) {
    if (truncated) {
      *truncated = (iter != parts_map.end());
    }
  } else {
    /* rebuild a map with only num_parts entries */
    std::map<uint32_t, std::unique_ptr<MultipartPart>> new_parts;
    std::map<uint32_t, std::unique_ptr<MultipartPart>>::iterator piter;
    for (i = 0, piter = parts.begin();
	 i < num_parts && piter != parts.end();
	 ++i, ++piter) {
      last_num = piter->first;
      new_parts[piter->first] = std::move(piter->second);
    }

    if (truncated) {
      *truncated = (piter != parts.end());
    }

    parts.swap(new_parts);
  }

  if (next_marker) {
    *next_marker = last_num;
  }

  return 0;
}

int MultipartUpload::get_info(const DoutPrefixProvider *dpp, optional_yield y, RGWObjectCtx* obj_ctx, rgw_placement_rule** rule, rgw::sal::Attrs* attrs)
{
  if (!rule && !attrs) {
    return 0;
  }

  if (rule) {
    if (!placement.empty()) {
      *rule = &placement;
      if (!attrs) {
        /* Don't need attrs, done */
	    return 0;
      }
    } else {
      *rule = nullptr;
    }
  }

  /* We need either attributes or placement, so we need a read */
  std::unique_ptr<rgw::sal::Object> meta_obj;
  meta_obj = get_meta_obj();
  meta_obj->set_in_extra_data(true);

  multipart_upload_info upload_info;
  bufferlist headbl;

  /* Read the obj head which contains the multipart_upload_info */
  std::unique_ptr<rgw::sal::Object::ReadOp> read_op = meta_obj->get_read_op(obj_ctx);
  meta_obj->set_prefetch_data(obj_ctx);

  int ret = read_op->prepare(y, dpp);
  if (ret < 0) {
    if (ret == -ENOENT) {
      return -ERR_NO_SUCH_UPLOAD;
    }
    return ret;
  }

  if (attrs) {
    /* Attrs are filled in by prepare */
    *attrs = meta_obj->get_attrs();
    if (!rule || *rule != nullptr) {
      /* placement was cached; don't actually read */
      return 0;
    }
  }

  /* Now read the placement from the head */
  ret = read_op->read(0, store->ctx()->_conf->rgw_max_chunk_size, headbl, y, dpp);
  if (ret < 0) {
    if (ret == -ENOENT) {
      return -ERR_NO_SUCH_UPLOAD;
    }
    return ret;
  }

  if (headbl.length() <= 0) {
    return -ERR_NO_SUCH_UPLOAD;
  }

  /* Decode multipart_upload_info */
  auto hiter = headbl.cbegin();
  try {
    decode(upload_info, hiter);
  } catch (buffer::error& err) {
    ldpp_dout(dpp, 0) << "ERROR: failed to decode multipart upload info" << dendl;
    return -EIO;
  }
  placement = upload_info.dest_placement;
  *rule = &placement;

  return 0;
}
}
