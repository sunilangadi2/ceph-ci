#pragma once

#include <string>
#include "common/async/yield_context.h"

class lua_State;
class rgw_user;
namespace rgw::sal {
  class RGWRadosStore;
}

namespace rgw::lua {

enum class context {
  preRequest,
  postRequest,
  none
};

// get context enum from string 
// the expected string the same as the enum (case insensitive)
// return "none" if not matched
context to_context(const std::string& s);

// verify a lua script
bool verify(const std::string& script, std::string& err_msg);

// store a lua script in a context
int write_script(rgw::sal::RGWRadosStore* store, const std::string& tenant, optional_yield y, context ctx, const std::string& script);

// read the stored lua script from a context
int read_script(rgw::sal::RGWRadosStore* store, const std::string& tenant, optional_yield y, context ctx, std::string& script);

// delete the stored lua script from a context
int delete_script(rgw::sal::RGWRadosStore* store, const std::string& tenant, optional_yield y, context ctx);

#ifdef WITH_RADOSGW_LUA_MODULES
#include <set>

using modules_t = std::set<std::string>;

// add a lua module to the allowlist
int add_module(rgw::sal::RGWRadosStore* store, optional_yield y, const std::string& module_name, bool allow_compilation);

// remove a lua module from the allowlist
int remove_module(rgw::sal::RGWRadosStore* store, optional_yield y, const std::string& module_name);

// list lua modules in the allowlist
int list_modules(rgw::sal::RGWRadosStore* store, optional_yield y, modules_t& modules);

// install all modules from the allowlist
// return the list of modules that failed to install and the output of the install command
int install_modules(rgw::sal::RGWRadosStore* store, optional_yield y, modules_t& failed_modules, std::string& output);
#endif
}

