// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:nil -*-
// vim: ts=8 sw=2 smarttab expandtab

#include <seastar/core/future.hh>

#include "messages/MOSDOp.h"
#include "messages/MOSDOpReply.h"

#include "crimson/common/exception.h"
#include "crimson/osd/pg.h"
#include "crimson/osd/osd.h"
#include "common/Formatter.h"
#include "crimson/osd/osd_operations/client_request.h"
#include "crimson/osd/osd_connection_priv.h"

namespace {
  seastar::logger& logger() {
    return crimson::get_logger(ceph_subsys_osd);
  }
}

namespace crimson::osd {

ClientRequest::ClientRequest(
  OSD &osd, crimson::net::ConnectionRef conn, Ref<MOSDOp> &&m)
  : osd(osd), conn(conn), m(m), ors(get_osd_priv(conn.get()).opSequencer)
{}

ClientRequest::~ClientRequest()
{
  logger().debug("{}: destroying", *this);
}

void ClientRequest::print(std::ostream &lhs) const
{
  lhs << *m;
}

void ClientRequest::dump_detail(Formatter *f) const
{
}

ClientRequest::ConnectionPipeline &ClientRequest::cp()
{
  return get_osd_priv(conn.get()).client_request_conn_pipeline;
}

ClientRequest::PGPipeline &ClientRequest::pp(PG &pg)
{
  return pg.client_request_pg_pipeline;
}

bool ClientRequest::is_pg_op() const
{
  return std::any_of(
    begin(m->ops), end(m->ops),
    [](auto& op) { return ceph_osd_op_type_pg(op.op.op); });
}

seastar::future<> ClientRequest::start()
{
  logger().debug("{}: start", *this);

  IRef opref = this;
  return seastar::repeat([this, opref]() mutable {
      logger().debug("{}: in repeat", *this);
      return with_blocking_future(handle.enter(cp().await_map))
      .then([this]() {
	return with_blocking_future(
	    osd.osdmap_gate.wait_for_map(
	      m->get_min_epoch()));
      }).then([this](epoch_t epoch) {
	return with_blocking_future(handle.enter(cp().get_pg));
      }).then([this] {
	return with_blocking_future(osd.wait_for_pg(m->get_spg()));
      }).then([this, opref](Ref<PG> pgref) mutable {
	return interruptor::with_interruption([this, opref, pgref]() mutable {
	  epoch_t same_interval_since = pgref->get_interval_start_epoch();
	  logger().debug("{} same_interval_since: {}", *this, same_interval_since);
	  return ors.preserve_sequence(
	    handle, same_interval_since, opref, pgref->get_pgid(),
	    interruptor::wrap_function(
	      [this, opref, pgref]() -> interruptible_future<> {
	      PG &pg = *pgref;
	      if (pg.can_discard_op(*m)) {
		logger().debug("{} op discarded, {}, same_primary_since: {}", *this, pg, pg.get_info().history.same_primary_since);
		return osd.send_incremental_map(conn, m->get_map_epoch());
	      }
	      return with_blocking_future_interruptible<IOInterruptCondition>(
		handle.enter(pp(pg).await_map)
	      ).then_interruptible([this, &pg]() mutable {
		return with_blocking_future_interruptible<IOInterruptCondition>(
		    pg.osdmap_gate.wait_for_map(m->get_min_epoch()));
	      }).then_interruptible([this, &pg](auto map) mutable {
		return with_blocking_future_interruptible<IOInterruptCondition>(
		    handle.enter(pp(pg).wait_for_active));
	      }).then_interruptible([this, &pg]() mutable {
		return with_blocking_future_interruptible<IOInterruptCondition>(
		    pg.wait_for_active_blocker.wait());
	      }).then_interruptible([this, pgref=std::move(pgref)]() mutable {
		if (m->finish_decode()) {
		  m->clear_payload();
		}
		if (is_pg_op()) {
		  return process_pg_op(pgref);
		} else {
		  return process_op(pgref);
		}
	      });
	    })
	  ).then_interruptible([this, opref, pgref]() mutable {
	    ors.operation_finished(opref, pgref->get_pgid());
	    return seastar::stop_iteration::yes;
	  });
	}, [this, opref, pgref](std::exception_ptr eptr) mutable {
	  if (*eptr.__cxa_exception_type() ==
	      typeid(::crimson::common::actingset_changed)) {
	    try {
	      std::rethrow_exception(eptr);
	    } catch(::crimson::common::actingset_changed& e) {
	      if (e.is_primary()) {
		logger().debug("{} operation restart, acting set changed", *this);
		return seastar::stop_iteration::no;
	      } else {
		logger().debug("{} operation abort, up primary changed", *this);
		ors.operation_finished(opref, pgref->get_pgid(), true);
		return seastar::stop_iteration::yes;
	      }
	    }
	  }
	  assert(*eptr.__cxa_exception_type() ==
	  typeid(crimson::common::system_shutdown_exception));
	  crimson::get_logger(ceph_subsys_osd).debug(
	      "{} operation skipped, system shutdown", *this);
	  return seastar::stop_iteration::yes;
	}, pgref);
      });
    });
}

ClientRequest::interruptible_future<>
ClientRequest::process_pg_op(
  Ref<PG> &pg)
{
  return pg->do_pg_ops(m)
    .then_interruptible([this, pg=std::move(pg)](Ref<MOSDOpReply> reply) {
      return conn->send(reply);
    });
}

ClientRequest::interruptible_future<>
ClientRequest::process_op(
  Ref<PG> &pgref)
{
  PG& pg = *pgref;
  return with_blocking_future_interruptible<IOInterruptCondition>(
      handle.enter(pp(pg).recover_missing)
  ).then_interruptible([this, &pg, pgref] {
    eversion_t ver;
    const hobject_t& soid = m->get_hobj();
    logger().debug("{} check for recovery, {}", *this, soid);
    if (pg.is_unreadable_object(soid, &ver) ||
	pg.is_degraded_or_backfilling_object(soid)) {
      logger().debug("{} need to wait for recovery, {}", *this, soid);
      if (pg.get_recovery_backend()->is_recovering(soid)) {
	return pg.get_recovery_backend()->get_recovering(soid).wait_for_recovered();
      } else {
	auto [op, fut] = osd.get_shard_services().start_operation<UrgentRecovery>(
			    soid, ver, pgref, osd.get_shard_services(), pg.get_osdmap_epoch());
	return std::move(fut);
      }
    }
    return seastar::now();
  }).then_interruptible([this, &pg] {
    return pg.already_complete(m->get_reqid()).then_unpack_interruptible(
      [this, &pg](bool completed, int ret)
      -> PG::interruptible_load_obc_ertr::future<> {
      if (completed) {
	auto reply = make_message<MOSDOpReply>(m.get(),
					       ret,
					       pg.get_osdmap_epoch(),
					       0,
					       false);
	reply->add_flags(CEPH_OSD_FLAG_ACK | CEPH_OSD_FLAG_ONDISK);
	return conn->send(std::move(reply));
      } else {
	return with_blocking_future_interruptible<IOInterruptCondition>(
          handle.enter(pp(pg).get_obc)).then_interruptible(
          [this, &pg]() -> PG::interruptible_load_obc_ertr::future<> {
	  logger().debug("{} to do ops", *this);
	  op_info.set_from_op(&*m, *pg.get_osdmap());
	  return pg.with_locked_obc(m, op_info, this, [this, &pg](auto obc) {
	    logger().debug("{}: got obc lock", *this);
	    return with_blocking_future_interruptible<IOInterruptCondition>(
	      handle.enter(pp(pg).process)
	    ).then_interruptible([this, &pg, obc]()
	      -> interruptible_future<Ref<MOSDOpReply>> {
	      if (!pg.is_primary()) {
		// primary can handle both normal ops and balanced reads
		if (is_misdirected(pg)) {
		  logger().trace("process_op: dropping misdirected op");
		  return seastar::make_ready_future<Ref<MOSDOpReply>>();
		} else if (const hobject_t& hoid = m->get_hobj();
			   !pg.get_peering_state().can_serve_replica_read(hoid)) {
		  auto reply = make_message<MOSDOpReply>(
		    m.get(), -EAGAIN, pg.get_osdmap_epoch(),
		    m->get_flags() & (CEPH_OSD_FLAG_ACK|CEPH_OSD_FLAG_ONDISK),
		    !m->has_flag(CEPH_OSD_FLAG_RETURNVEC));
		  return seastar::make_ready_future<Ref<MOSDOpReply>>(std::move(reply));
		}
	      }
	      return pg.do_osd_ops(m, obc, op_info);
	    }).then_interruptible([this](Ref<MOSDOpReply> reply) {
	      if (reply) {
		return conn->send(std::move(reply));
	      } else {
		return seastar::now();
	      }
	    });
	  });
	});
      }
    });
  }).safe_then_interruptible([pgref=std::move(pgref)] {
    return seastar::now();
  }, PG::load_obc_ertr::all_same_way([](auto &code) {
    logger().error("ClientRequest saw error code {}", code);
    return seastar::now();
  }));
}

bool ClientRequest::is_misdirected(const PG& pg) const
{
  // otherwise take a closer look
  if (const int flags = m->get_flags();
      flags & CEPH_OSD_FLAG_BALANCE_READS ||
      flags & CEPH_OSD_FLAG_LOCALIZE_READS) {
    if (!op_info.may_read()) {
      // no read found, so it can't be balanced read
      return true;
    }
    if (op_info.may_write() || op_info.may_cache()) {
      // write op, but i am not primary
      return true;
    }
    // balanced reads; any replica will do
    return pg.is_nonprimary();
  }
  // neither balanced nor localize reads
  return true;
}

}
