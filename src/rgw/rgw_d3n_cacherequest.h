// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab ft=cpp

#ifndef RGW_CACHEREQUEST_H
#define RGW_CACHEREQUEST_H

#include <fcntl.h>
#include <stdlib.h>
#include <aio.h>

#include "common/Semaphore.h"
#include "include/rados/librados.hpp"
#include "include/Context.h"
#include "common/async/completion.h"

#include <errno.h>
#include "common/error_code.h"
#include "common/errno.h"

#include "rgw_aio.h"
#include "rgw_cache.h"


struct D3nGetObjData {
  std::mutex d3n_lock;
  Semaphore d3n_sem;
  atomic_ulong d3n_libaio_op_seq{0};
  atomic_ulong d3n_libaio_op_prev{0};
  std::condition_variable d3n_libaio_op_cv;
  std::mutex d3n_libaio_op_cv_lock;
};

class D3nCacheRequest {
  public:
    std::mutex lock;
    buffer::list* bl{nullptr};
    std::string oid;
    std::string key;
    off_t ofs{0};
    off_t len{0};
    off_t read_ofs{0};
    rgw::AioResult* r{nullptr};
    rgw::Aio* aio{nullptr};

    virtual ~D3nCacheRequest() {};
    virtual void d3n_libaio_release()=0;
    virtual void d3n_libaio_cancel_io()=0;
    virtual int d3n_libaio_status()=0;
    virtual void d3n_libaio_finish()=0;
};

struct D3nL1CacheRequest : public D3nCacheRequest {
  using sigval_cb = void (*) (sigval_t);
  int stat{-1};
  int ret{0};
  struct aiocb d3n_aiocb;
  std::mutex* d_lock{nullptr};
  Semaphore* d_sem;
  atomic_ulong* d_libaio_op_curr{nullptr};
  atomic_ulong* d_libaio_op_prev{nullptr};
  atomic_ulong libaio_op_seq{0};
  std::condition_variable* d_libaio_op_cv{nullptr};
  std::mutex* d_libaio_op_cv_lock{nullptr};

  ~D3nL1CacheRequest() {
    const std::lock_guard l(lock);
    if (d3n_aiocb.aio_buf != nullptr) {
      free((void*)d3n_aiocb.aio_buf);
      d3n_aiocb.aio_buf = nullptr;
    }
    ::close(d3n_aiocb.aio_fildes);

    lsubdout(g_ceph_context, rgw_datacache, 30) << "D3nDataCache: " << __func__ << "(): Read From Cache, comlete" << dendl;
  }

  int d3n_execute_io_op(std::string obj_key, bufferlist* bl, int read_len, int ofs, int read_ofs, std::string& cache_location,
                        sigval_cb f, rgw::Aio* aio, rgw::AioResult* r) {
    std::string location = cache_location + "/" + obj_key;
    lsubdout(g_ceph_context, rgw_datacache, 20) << "D3nDataCache: " << __func__ << "(): Read From Cache, location='" << location << "', ofs=" << ofs << ", read_ofs=" << read_ofs << " read_len=" << read_len << dendl;
    int rfd;
    if ((rfd = ::open(location.c_str(), O_RDONLY)) == -1) {
      lsubdout(g_ceph_context, rgw, 0) << "D3nDataCache: Error: " << __func__ << "():  ::open(" << location << ") errno=" << errno << dendl;
      return -errno;
    }
    if((ret = posix_fadvise(rfd, 0, 0, g_conf()->rgw_d3n_l1_fadvise)) != 0) {
      lsubdout(g_ceph_context, rgw, 0) << "D3nDataCache: Warning: " << __func__ << "()  posix_fadvise( , , , "  << g_conf()->rgw_d3n_l1_fadvise << ") ret=" << ret << dendl;
    }
    if ((read_ofs > 0) && (::lseek(rfd, read_ofs, SEEK_SET) != read_ofs)) {
      lsubdout(g_ceph_context, rgw, 0) << "D3nDataCache: Error: " << __func__ << "()  ::lseek(" << location << ", read_ofs=" << read_ofs << ") errno=" << errno << dendl;
      return -errno;
    }
    char* io_buf = (char*)malloc(read_len);
    if (io_buf == NULL) {
      lsubdout(g_ceph_context, rgw, 0) << "D3nDataCache: Error: " << __func__ << "()  malloc(" << read_len << ") errno=" << errno << dendl;
      return -errno;
    }
    ssize_t nbytes;
    if ((nbytes = ::read(rfd, io_buf, read_len)) == -1) {
      lsubdout(g_ceph_context, rgw, 0) << "D3nDataCache: Error: " << __func__ << "()  ::read(" << location << ", read_ofs=" << read_ofs << ", read_len=" << read_len << ") errno=" << errno << dendl;
      free(io_buf);
      return -errno;
    }
    if (nbytes != read_len) {
      lsubdout(g_ceph_context, rgw, 0) << "D3nDataCache: Error: " << __func__ << "()  ::read(" << location << ", read_ofs=" << read_ofs << ", read_len=" << read_len << ") read_len!=nbytes = " << nbytes << dendl;
      free(io_buf);
      return -EIO;
    }
    lsubdout(g_ceph_context, rgw_datacache, 30) << "D3nDataCache: " << __func__ << "(): Read From Cache, nbytes=" << nbytes << dendl;
    bl->append(io_buf, nbytes);
    r->result = 0;
    aio->put(*(r));
    ::close(rfd);
    free(io_buf);
    return 0;
  }

  int d3n_prepare_libaio_read_op(std::string obj_key, bufferlist* _bl, int read_len, int _ofs, int _read_ofs, std::string& cache_location,
                        sigval_cb cbf, rgw::Aio* _aio, rgw::AioResult* _r, D3nGetObjData* d_d3n_data) {
    std::string location = cache_location + "/" + obj_key;
    lsubdout(g_ceph_context, rgw_datacache, 20) << "D3nDataCache: " << __func__ << "(): Read From Cache, location='" << location << "', ofs=" << ofs << ", read_ofs=" << read_ofs << " read_len=" << read_len << dendl;
    r = _r;
    aio = _aio;
    bl = _bl;
    ofs = _ofs;
    key = obj_key;
    len = read_len;
    stat = EINPROGRESS;
    d_lock = &d_d3n_data->d3n_lock;
    d_sem = &d_d3n_data->d3n_sem;
    d_libaio_op_curr = &d_d3n_data->d3n_libaio_op_seq;
    d_libaio_op_prev = &d_d3n_data->d3n_libaio_op_prev;
    d_libaio_op_cv = &d_d3n_data->d3n_libaio_op_cv;
    d_libaio_op_cv_lock = &d_d3n_data->d3n_libaio_op_cv_lock;

    memset(&d3n_aiocb, 0, sizeof(d3n_aiocb));
    d3n_aiocb.aio_fildes = ::open(location.c_str(), O_RDONLY);
    if (d3n_aiocb.aio_fildes < 0) {
      lsubdout(g_ceph_context, rgw, 0) << "Error: " << __func__ << " ::open(" << cache_location << ")" << dendl;
      return -errno;
    }
    if (g_conf()->rgw_d3n_l1_fadvise != POSIX_FADV_NORMAL)
      posix_fadvise(d3n_aiocb.aio_fildes, 0, 0, g_conf()->rgw_d3n_l1_fadvise);

    d3n_aiocb.aio_buf = (volatile void*)malloc(read_len);
    d3n_aiocb.aio_nbytes = read_len;
    d3n_aiocb.aio_offset = read_ofs;
    d3n_aiocb.aio_sigevent.sigev_notify = SIGEV_THREAD;
    d3n_aiocb.aio_sigevent.sigev_notify_function = cbf;
    d3n_aiocb.aio_sigevent.sigev_notify_attributes = nullptr;

    d3n_aiocb.aio_sigevent.sigev_value.sival_ptr = this;
    return 0;
  }

  void d3n_libaio_release() {}

  void d3n_libaio_cancel_io() {
    const std::lock_guard l(lock);
    stat = ECANCELED;
  }

  int d3n_libaio_status() {
    const std::lock_guard l(lock);
    lsubdout(g_ceph_context, rgw_datacache, 30) << "D3nDataCache: " << __func__ << "(): key=" << key << ", stat=" << stat << dendl;
    if (stat != EINPROGRESS) {
      if (stat == ECANCELED) {
        lsubdout(g_ceph_context, rgw, 2) << "D3nDataCache: " << __func__ << "(): stat == ECANCELED" << dendl;
        return ECANCELED;
      }
    }
    stat = aio_error(&d3n_aiocb);
    return stat;
  }

  void d3n_libaio_finish() {
    const std::lock_guard l(lock);
    lsubdout(g_ceph_context, rgw_datacache, 20) << "D3nDataCache: " << __func__ << "(): Read From Cache, libaio callback - returning data: key=" << key << ", aio_nbytes=" << d3n_aiocb.aio_nbytes << dendl;
    bl->append((char*)d3n_aiocb.aio_buf, d3n_aiocb.aio_nbytes);
  }




  /// unique_ptr with custom deleter for struct aiocb
  struct d3n_libaio_AioCBDeleter {
    void operator()(struct aiocb* c) {
          if(c->aio_fildes > 0) ::close(c->aio_fildes);
          delete c;
    }
  };

  using unique_aio_cb_ptr = std::unique_ptr<struct aiocb, d3n_libaio_AioCBDeleter>;

  template <typename Result>
  struct AsyncFileReadOp {
    bufferlist result;
    unique_aio_cb_ptr aio_cb;
    using Signature = void(boost::system::error_code, Result);
    using Completion = ceph::async::Completion<Signature, AsyncFileReadOp<Result>>;
    
    int init(std::string file_path, off_t read_ofs, off_t read_len, void* arg) {
      aio_cb.reset(new struct aiocb);
      memset(aio_cb.get(), 0, sizeof(struct aiocb));
      aio_cb->aio_fildes = TEMP_FAILURE_RETRY(::open(file_path.c_str(), O_RDONLY|O_CLOEXEC|O_BINARY));
      if(aio_cb->aio_fildes < 0) {
        int err = errno;
        lsubdout(g_ceph_context, rgw, 2) << "IOC:AsyncFileReadOp can't open " << file_path << ": " << cpp_strerror(err) << dendl;
        return -err;
      }
      posix_fadvise(aio_cb->aio_fildes, 0, 0, POSIX_FADV_DONTNEED);

      bufferptr bp(read_len);
      aio_cb->aio_buf = bp.c_str();
      result.append(std::move(bp));

      aio_cb->aio_nbytes = read_len;
      aio_cb->aio_offset = read_ofs;
      aio_cb->aio_sigevent.sigev_notify = SIGEV_THREAD;
      aio_cb->aio_sigevent.sigev_notify_function = aio_dispatch;
      aio_cb->aio_sigevent.sigev_notify_attributes = nullptr;
      aio_cb->aio_sigevent.sigev_value.sival_ptr = arg;

      return 0;
    }

    static void aio_dispatch(sigval_t sigval) {
      auto p = std::unique_ptr<Completion>{static_cast<Completion*>(sigval.sival_ptr)};
      auto op = std::move(p->user_data);
      const int ret = -aio_error(op.aio_cb.get());
      boost::system::error_code ec;
      if (ret < 0) {
          ec.assign(-ret, boost::system::system_category());
      }

      ceph::async::dispatch(std::move(p), ec, std::move(op.result));
    }

    template <typename Executor1, typename CompletionHandler>
    static auto create(const Executor1& ex1, CompletionHandler&& handler) {
      auto p = Completion::create(ex1, std::move(handler));
      return p;
    }
  };

  template <typename ExecutionContext, typename CompletionToken>
  auto async_read(ExecutionContext& ctx, const std::string& file_path,
                  off_t read_ofs, off_t read_len, CompletionToken&& token) {
    using Op = AsyncFileReadOp<bufferlist>;
    using Signature = typename Op::Signature;
    boost::asio::async_completion<CompletionToken, Signature> init(token);
    auto p = Op::create(ctx.get_executor(), init.completion_handler);
    auto& op = p->user_data;
    int ret = op.init(file_path, read_ofs, read_len, p.get());
    if(0 == ret) {
      ret = ::aio_read(op.aio_cb.get());
    }

    if(ret < 0) {
      auto ec = boost::system::error_code{-ret, boost::system::system_category()};
      ceph::async::post(std::move(p), ec, bufferlist{});
    } else {
      p.release();
    }
    return init.result.get();
    // ldout(g_ceph_context, 1) << "#MK# IOCDataCache: " << __func__ << "(): >>>" << dendl;
    // auto r = init.result.get();
    // ldout(g_ceph_context, 1) << "#MK# IOCDataCache: " << __func__ << "(): <<" << dendl;
    // return r;
  }

  struct d3n_libaio_handler {
    rgw::Aio* throttle = nullptr;
    rgw::AioResult& r;
    // read callback
    void operator()(boost::system::error_code ec, bufferlist bl) const {
      r.result = -ec.value();
      r.data = std::move(bl);
      throttle->put(r);
    }
  };

  void file_aio_read_abstract(boost::asio::io_context& context, spawn::yield_context yield,
                              std::string& file_path, off_t read_ofs, off_t read_len,
                              rgw::Aio* aio, rgw::AioResult& r) {
    using namespace boost::asio;
    async_completion<spawn::yield_context, void()> init(yield);
    auto ex = get_associated_executor(init.completion_handler);
    lsubdout(g_ceph_context, rgw_datacache, 1) << "#MK# IOCDataCache: " << __func__ << "():  >> async_read()" << dendl;
    
    //async_read(context, file_path, read_ofs, read_len, bind_executor(ex, d3n_libaio_handler{aio, r}));
    auto& ref = r.obj.get_ref();
    async_read(context, file_path+"/"+ref.obj.oid, read_ofs, read_len, bind_executor(ex, d3n_libaio_handler{aio, r}));
    
    lsubdout(g_ceph_context, rgw_datacache, 1) << "#MK# IOCDataCache: " << __func__ << "():  << async_read()" << dendl;
  }


};

#endif
