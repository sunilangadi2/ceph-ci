// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab

#ifndef CEPH_COMMON_JOURNALD_H
#define CEPH_COMMON_JOURNALD_H

#include <memory>
#include <sys/types.h>
#include <sys/socket.h>

namespace ceph {

namespace logging {

namespace detail {
class EntryEncoder;

class JournaldClient {
 public:
  JournaldClient();
  ~JournaldClient();
  int send();
  struct msghdr m_msghdr;
 private:
  int fd;

  enum class MemFileMode;
  MemFileMode mem_file_mode;

  void detect_mem_file_mode();
  int open_mem_file();
};
}

class Entry;
class SubsystemMap;

/**
 * Logger to send local logs to journald
 * 
 * local logs means @code dout(0) << ... @endcode and similars
 */
class JournaldLogger {
 public:
  JournaldLogger(const SubsystemMap *s);
  ~JournaldLogger();

  /**
   * @returns 0 if log entry is successfully sent, -1 otherwise.
   */
  int log_entry(const Entry &e);

 private:
  detail::JournaldClient client;

  std::unique_ptr<detail::EntryEncoder> m_entry_encoder;

  const SubsystemMap * m_subs;
};


}
}

#endif
