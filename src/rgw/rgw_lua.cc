#include <lua.hpp>
#include "services/svc_zone.h"
#include "services/svc_sys_obj.h"
#include "common/dout.h"
#include "rgw_lua_utils.h"
#include "rgw_sal_rados.h"
#include "rgw_lua.h"
#ifdef WITH_RADOSGW_LUA_MODULES
#include <boost/process.hpp>
#include <boost/filesystem.hpp>
#include "rgw_lua_version.h"
#endif

#define dout_subsys ceph_subsys_rgw

namespace rgw::lua {

context to_context(const std::string& s) 
{
  if (strcasecmp(s.c_str(), "prerequest") == 0) {
    return context::preRequest;
  }
  if (strcasecmp(s.c_str(), "postrequest") == 0) {
    return context::postRequest;
  }
  return context::none;
}

std::string to_string(context ctx) 
{
  switch (ctx) {
    case context::preRequest:
      return "prerequest";
    case context::postRequest:
      return "postrequest";
    case context::none:
      break;
  }
  return "none";
}

bool verify(const std::string& script, std::string& err_msg) 
{
  lua_State *L = luaL_newstate();
  lua_state_guard guard(L);
  luaL_openlibs(L);
  try {
    if (luaL_loadstring(L, script.c_str()) != LUA_OK) {
      err_msg.assign(lua_tostring(L, -1));
      return false;
    }
  } catch (const std::runtime_error& e) {
    err_msg = e.what();
    return false;
  }
  err_msg = "";
  return true;
}

std::string script_oid(context ctx, const std::string& tenant) {
  static const std::string SCRIPT_OID_PREFIX("script.");
  return SCRIPT_OID_PREFIX + to_string(ctx) + "." + tenant;
}


int read_script(rgw::sal::RGWRadosStore* store, const std::string& tenant, optional_yield y, context ctx, std::string& script)
{
  RGWSysObjectCtx obj_ctx(store->svc()->sysobj->init_obj_ctx());
  RGWObjVersionTracker objv_tracker;

  rgw_raw_obj obj(store->svc()->zone->get_zone_params().log_pool, script_oid(ctx, tenant));

  bufferlist bl;
  
  const auto rc = rgw_get_system_obj(
      obj_ctx,
      obj.pool, 
      obj.oid,
      bl,
      &objv_tracker,
      nullptr, 
      y, 
      nullptr, 
      nullptr);

  if (rc < 0) {
    return rc;
  }

  auto iter = bl.cbegin();
  try {
    ceph::decode(script, iter);
  } catch (buffer::error& err) {
    return -EIO;
  }

  return 0;
}

int write_script(rgw::sal::RGWRadosStore* store, const std::string& tenant, optional_yield y, context ctx, const std::string& script)
{
  RGWSysObjectCtx obj_ctx(store->svc()->sysobj->init_obj_ctx());
  RGWObjVersionTracker objv_tracker;

  rgw_raw_obj obj(store->svc()->zone->get_zone_params().log_pool, script_oid(ctx, tenant));

  bufferlist bl;
  ceph::encode(script, bl);

  const auto rc = rgw_put_system_obj(
      obj_ctx, 
      obj.pool, 
      obj.oid,
      bl,
      false, 
      &objv_tracker,
      real_time());

  if (rc < 0) {
    return rc;
  }

  return 0;
}

int delete_script(rgw::sal::RGWRadosStore* store, const std::string& tenant, optional_yield y, context ctx)
{
  RGWObjVersionTracker objv_tracker;

  rgw_raw_obj obj(store->svc()->zone->get_zone_params().log_pool, script_oid(ctx, tenant));

  const auto rc = rgw_delete_system_obj(
      store->svc()->sysobj, 
      obj.pool, 
      obj.oid,
      &objv_tracker);

  if (rc < 0 && rc != -ENOENT) {
    return rc;
  }

  return 0;
}

#ifdef WITH_RADOSGW_LUA_MODULES

const std::string MODULE_LIST_OBJECT_NAME = "lua_modules_allowlist";

namespace bp = boost::process;

int add_module(rgw::sal::RGWRadosStore* store, optional_yield y, const std::string& module_name, bool allow_compilation) {
  // verify that luarocks can load this module
  const auto p = bp::search_path("luarocks");
  if (p.empty()) {
    return -ECHILD;
  }
  bp::ipstream is;
  const auto cmd = p.string() + " search --porcelain" + (allow_compilation ? " " : " --binary ") + module_name;
  bp::child c(cmd,
      bp::std_in.close(),
      bp::std_err > bp::null,
      bp::std_out > is);

  std::string line;
  bool module_found = false;
  // TODO: yield on reading the output
  while (c.running() && std::getline(is, line) && !line.empty()) {
    module_found = true;
  }
  c.wait();
  auto ret = c.exit_code();
  if (ret) {
    return -ret;
  }

  if (!module_found) {
    return -EINVAL;
  }
  
  // add module to list
  const bufferlist empty_bl;
  std::map<std::string, bufferlist> new_module{{module_name, empty_bl}};
  librados::ObjectWriteOperation op;
  op.omap_set(new_module);
  ret = rgw_rados_operate(*(store->getRados()->get_lc_pool_ctx()), 
      MODULE_LIST_OBJECT_NAME, &op, y);

  if (ret < 0) {
    return ret;
  } 
  return 0;
}

int remove_module(rgw::sal::RGWRadosStore* store, optional_yield y, const std::string& module_name) {
  librados::ObjectWriteOperation op;
  op.omap_rm_keys(std::set<std::string>({module_name}));
  const auto ret = rgw_rados_operate(*(store->getRados()->get_lc_pool_ctx()), 
    MODULE_LIST_OBJECT_NAME, &op, y);

  if (ret < 0) {
    return ret;
  }

  return 0;
}

int list_modules(rgw::sal::RGWRadosStore* store, optional_yield y, modules_t& modules) {
  constexpr auto max_chunk = 1024U;
  std::string start_after;
  bool more = true;
  int rval;
  while (more) {
    librados::ObjectReadOperation op;
    modules_t modules_chunk;
    op.omap_get_keys2(start_after, max_chunk, &modules_chunk, &more, &rval);
    const auto ret = rgw_rados_operate(*(store->getRados()->get_lc_pool_ctx()),
      MODULE_LIST_OBJECT_NAME, &op, nullptr, y);
  
    if (ret < 0) {
      return ret;
    }

    modules.merge(modules_chunk);
  }
 
  return 0;
}

int install_modules(rgw::sal::RGWRadosStore* store, optional_yield y, modules_t& failed_modules, std::string& output) {
  // luarocks directory cleanup
  const auto& luarocks_location = g_conf().get_val<std::string>("rgw_luarocks_location");
  boost::system::error_code ec;
  boost::filesystem::remove_all(luarocks_location, ec);
  if (ec.value() != 0 && ec.value() != ENOENT) {
    output.append("failed to clear luarock directory: ");
    output.append(ec.message());
    output.append("\n");
    return ec.value();
  }

  modules_t modules;
  auto ret = list_modules(store, y, modules);
  if (ret == -ENOENT) {
    // allowlist is empty 
    return 0;
  }
  if (ret < 0) {
    return ret;
  }
  // verify that luarocks exists
  const auto p = bp::search_path("luarocks");
  if (p.empty()) {
    return -ECHILD;
  }

  // the lua rocks install dir will be created by luarocks the first time it is called
  std::for_each(modules.begin(), modules.end(), [&luarocks_location, &p, &failed_modules, &output](const std::string& module_name) {
    bp::ipstream is;
    bp::child c(p, "install", "--lua-version", CEPH_LUA_VERSION, "--tree", luarocks_location, "--deps-mode", "one", module_name, 
        bp::std_in.close(),
        (bp::std_err & bp::std_out) > is);

    // TODO: yield until wait returns
    std::string line = "CMD: luarocks install --lua-version " + std::string(CEPH_LUA_VERSION) + std::string(" --tree ") + 
      luarocks_location + " --deps-mode one " + module_name;

    do {
      if (!line.empty()) {
        output.append(line);
        output.append("\n");
      }
    } while (c.running() && std::getline(is, line));

    c.wait();
    if (c.exit_code()) {
      failed_modules.insert(module_name);
    }
  });

  return 0;
}

#endif

}

