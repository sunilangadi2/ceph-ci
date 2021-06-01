#pragma once
#include <atomic>
#include <tbb/concurrent_unordered_map.h>
#include <chrono>
#include <thread>
#include <condition_variable>

#if !defined(dout_subsys)
#define dout_subsys ceph_subsys_rgw
#define def_dout_subsys
#endif

#define dout_context g_ceph_context
struct RGWQoSInfo {
  int64_t max_write_ops;
  int64_t max_read_ops;
  int64_t max_write_bytes;
  int64_t max_read_bytes;
  bool enabled = false;
  RGWQoSInfo()
    : max_write_ops(0), max_read_ops(0), max_write_bytes(0), max_read_bytes(0)  {}

  void encode(bufferlist& bl) const {
    ENCODE_START(1, 1, bl);
    encode(max_write_ops, bl);
    encode(max_read_ops, bl);
    encode(max_write_bytes, bl);
    encode(max_read_bytes, bl);
    encode(enabled, bl);
    ENCODE_FINISH(bl);
  }
  void decode(bufferlist::const_iterator& bl) {
    DECODE_START(1, bl);
    decode(max_write_ops,bl);
    decode(max_read_ops, bl);
    decode(max_write_bytes,bl);
    decode(max_read_bytes, bl);
    decode(enabled, bl);
    DECODE_FINISH(bl);
  }

  void dump(Formatter *f) const;

  void decode_json(JSONObj *obj);

};
WRITE_CLASS_ENCODER(RGWQoSInfo)
struct qos_entry {
  atomic_int64_t read[4] = {0};
  atomic_int64_t write[4] = {0};
};

class QosDatastruct {
  enum {
    RGW_QOS_OPS,
    RGW_QOS_CONCURR,
    RGW_QOS_BW,
    RGW_QOS_TS
  };
  tbb::concurrent_unordered_map<std::string, std::unique_ptr<qos_entry>> qos_entries;
  bool is_read_op(const char *method) {
    if((strcmp(method,"GET") == 0) || (strcmp(method,"HEAD") == 0))
      return true;
    return false;
  }
  int increase_entry_read(std::string tenant, uint64_t curr_timestamp) {
    auto it = qos_entries.find(tenant);
    if (it == qos_entries.end())
      it = qos_entries.insert(std::make_pair(tenant, std::unique_ptr<qos_entry>(new qos_entry()))).first;
    dout(20) << "rate limiting tenant: " << tenant << dendl;
    dout(20) << "rate limiting read ops: " << it->second->read[RGW_QOS_OPS] << dendl;
    dout(20) << "rate limiting read bytes: " << it->second->read[RGW_QOS_BW] << dendl;
    dout(20) << "rate limiting concurrency: " << it->second->read[RGW_QOS_CONCURR] << dendl;
    dout(20) << "rate limiting time diff is: " << (curr_timestamp - it->second->read[RGW_QOS_TS]) << dendl;
    if (curr_timestamp - it->second->read[RGW_QOS_TS] >= 1) {
      it->second->read[RGW_QOS_OPS] = 1;
      it->second->read[RGW_QOS_BW] = 0;
      it->second->read[RGW_QOS_CONCURR]++;
      it->second->read[RGW_QOS_TS] = curr_timestamp;
      return 0;
    }
      it->second->read[RGW_QOS_OPS]++;
      it->second->read[RGW_QOS_CONCURR]++;
      return 0;
  }
  int increase_entry_write(std::string tenant, uint64_t curr_timestamp) {
    auto it = qos_entries.find(tenant);
    if(it == qos_entries.end())
      it = qos_entries.insert(std::make_pair(tenant, std::unique_ptr<qos_entry>(new qos_entry()))).first;
    dout(20) << "rate limiting tenant: " << tenant << dendl;
    dout(20) << "rate limiting write ops: " << it->second->write[RGW_QOS_OPS] << dendl;
    dout(20) << "rate limiting write bytes: " << it->second->write[RGW_QOS_BW] << dendl;
    dout(20) << "rate limiting concurrency: " << it->second->write[RGW_QOS_CONCURR] << dendl;
    dout(20) << "rate limiting time diff is: " << (curr_timestamp - it->second->write[RGW_QOS_TS]) << dendl;
    if(curr_timestamp - it->second->write[RGW_QOS_TS] >= 1) {
      it->second->write[RGW_QOS_OPS] = 1;
      it->second->write[RGW_QOS_BW] = 0;
      it->second->write[RGW_QOS_CONCURR]++;
      it->second->write[RGW_QOS_TS] = curr_timestamp;
      return 0;
    }
      it->second->write[RGW_QOS_OPS]++;
      it->second->write[RGW_QOS_CONCURR]++;
      return 0;
  }
  bool check_entry_read(std::string tenant, int64_t ops_limit, int64_t bw_limit) {
    auto it = qos_entries.find(tenant);
    if(((it->second->read[RGW_QOS_OPS] > ops_limit ||
    it->second->read[RGW_QOS_CONCURR] > ops_limit) && (ops_limit > 0)) ||
    (it->second->read[RGW_QOS_BW] > bw_limit && bw_limit > 0))
      return false;
    return true;
  }
  bool check_entry_write(std::string tenant, int64_t ops_limit, int64_t bw_limit) {
    auto it =  qos_entries.find(tenant);
    if(((it->second->write[RGW_QOS_OPS] > ops_limit ||
    it->second->write[RGW_QOS_CONCURR] > ops_limit) && (ops_limit > 0)) ||
    (it->second->write[RGW_QOS_BW] > bw_limit && bw_limit > 0))
      return false;
    return true;
  }
  public:
    QosDatastruct(const QosDatastruct&) = delete;
    QosDatastruct& operator =(const QosDatastruct&) = delete;
    QosDatastruct(QosDatastruct&&) = delete;
    QosDatastruct& operator =(QosDatastruct&&) = delete;
    QosDatastruct() {};
    int increase_entry(const char *method, std::string tenant, uint64_t curr_timestamp) {
      if(tenant.empty() || tenant.length() == 1)
        return -ENOENT;
      bool is_read = is_read_op(method);
      if(is_read)
        return increase_entry_read(tenant, curr_timestamp);
      return increase_entry_write(tenant, curr_timestamp);
    }
    bool check_entry(const char *method, std::string tenant, RGWQoSInfo qos_info) {
      if (tenant.empty() || tenant.length() == 1 || !qos_info.enabled)
        return true;
      bool is_read = is_read_op(method);
      if (is_read)
        return check_entry_read(tenant, qos_info.max_read_ops, qos_info.max_read_bytes);
      return check_entry_write(tenant, qos_info.max_write_ops, qos_info.max_write_bytes);
    }
    bool decrease_concurrent_ops(const char *method, std::string tenant) {
      if(tenant.empty() || tenant.length() == 1)
        return false;
      bool is_read = is_read_op(method);
      auto it = qos_entries.find(tenant);
      if(it == qos_entries.end())
        it = qos_entries.insert(std::make_pair(tenant, std::unique_ptr<qos_entry>(new qos_entry()))).first;
      if (is_read && it->second->read[RGW_QOS_CONCURR] > 0)
        it->second->read[RGW_QOS_CONCURR]--;
      else if (it->second->write[RGW_QOS_CONCURR] > 0) {
        it->second->write[RGW_QOS_CONCURR]--;
      }
      dout(20) << "ratelimit read concurrency: " <<  it->second->read[RGW_QOS_CONCURR] <<
                  " write concurrency: " << it->second->write[RGW_QOS_CONCURR] << dendl;
      return true;
    }
    bool increase_bw(const char *method, std::string tenant, int64_t amount) {
      if(tenant.empty() || tenant.length() == 1)
        return false;
      bool is_read = is_read_op(method);
      auto it = qos_entries.find(tenant);
      if(it == qos_entries.end())
        it = qos_entries.insert(std::make_pair(tenant, std::unique_ptr<qos_entry>(new qos_entry()))).first;
      if (is_read)
        it->second->read[RGW_QOS_BW] += amount;
      else {
        it->second->write[RGW_QOS_BW] += amount;
      }
      dout(20) << "rate limiting read bytes is: " << it->second->read[RGW_QOS_BW] <<
                  " write bytes is: " <<  it->second->write[RGW_QOS_BW] << dendl;
      dout(20) << "rate limiting tenant is: " << tenant << " bytes added: " << amount << 
      " method is: " << method << dendl;
      return true;
    }
    void clear() {
      qos_entries.clear();
    }
};
class QosActiveDatastruct {
  std::atomic_uint8_t stopped = {false};
  std::condition_variable cv;
  std::mutex cv_m;
  std::thread runner;
  atomic_uint8_t current_active = 0;
  std::shared_ptr<QosDatastruct> qos[2];
  void replace_active() {
    std::unique_lock<std::mutex> lk(cv_m);
    while(cv.wait_for(lk, 10min) == std::cv_status::timeout && !stopped) {
      current_active = current_active ^ 1;
      dout(20) << "replacing active qos data structure" << dendl;
      while (qos[(current_active ^ 1)].use_count() > 1 && !stopped) {
        if (cv.wait_for(lk, 2min) != std::cv_status::timeout || stopped)
          return;
      }
      if (stopped)
        return;
      dout(20) << "clearing passive qos data structure" << dendl;
      qos[(current_active ^ 1)]->clear();
    }
  }
  public:
    QosActiveDatastruct(const QosActiveDatastruct&) = delete;
    QosActiveDatastruct& operator =(const QosActiveDatastruct&) = delete;
    QosActiveDatastruct(QosActiveDatastruct&&) = delete;
    QosActiveDatastruct& operator =(QosActiveDatastruct&&) = delete;
    QosActiveDatastruct() {
      qos[0] = std::shared_ptr<QosDatastruct>(new QosDatastruct());
      qos[1] = std::shared_ptr<QosDatastruct>(new QosDatastruct());
    }
    ~QosActiveDatastruct() {
      dout(20) << "stopping ratelimit_gc thread" << dendl;
      stopped = true;
      cv.notify_all();
      runner.join();
    }
    std::shared_ptr<QosDatastruct> get_active() {
      return qos[current_active];
    }
    void start() {
      dout(20) << "starting ratelimit_gc thread" << dendl;
      runner = std::thread(&QosActiveDatastruct::replace_active, this);
      const auto rc = ceph_pthread_setname(runner.native_handle(), "ratelimit_gc");
      ceph_assert(rc==0);
    }
};
#if defined(def_dout_subsys)
#undef def_dout_subsys
#undef dout_subsys
#endif
#undef dout_context