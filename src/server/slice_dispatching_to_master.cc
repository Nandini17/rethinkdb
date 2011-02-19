#include "replication/master.hpp"

#include "server/slice_dispatching_to_master.hpp"

using replication::master_t;

btree_slice_dispatching_to_master_t::btree_slice_dispatching_to_master_t(btree_slice_t *slice, snag_ptr_t<replication::master_t> master) : slice_(slice), master_(master) {
    master_->register_dispatcher(this);
}


/* set_store_t interface. */

get_result_t btree_slice_dispatching_to_master_t::get_cas(const store_key_t &key, castime_t castime) {
    on_thread_t th(slice_->home_thread);
    if (master_) coro_t::spawn_on_thread(master_->home_thread, boost::bind(&master_t::get_cas, master_.get(), key, castime));
    return slice_->get_cas(key, castime);
}

set_result_t btree_slice_dispatching_to_master_t::sarc(const store_key_t &key, data_provider_t *data, mcflags_t flags, exptime_t exptime, castime_t castime, add_policy_t add_policy, replace_policy_t replace_policy, cas_t old_cas) {
    on_thread_t th(slice_->home_thread);

    if (master_) {
        buffer_borrowing_data_provider_t borrower(master_->home_thread, data);
        coro_t::spawn_on_thread(master_->home_thread, boost::bind(&master_t::sarc, master_.get(), key, borrower.side_provider(), flags, exptime, castime, add_policy, replace_policy, old_cas));
        return slice_->sarc(key, &borrower, flags, exptime, castime, add_policy, replace_policy, old_cas);
    } else {
        return slice_->sarc(key, data, flags, exptime, castime, add_policy, replace_policy, old_cas);
    }
}

incr_decr_result_t btree_slice_dispatching_to_master_t::incr_decr(incr_decr_kind_t kind, const store_key_t &key, uint64_t amount, castime_t castime) {
    on_thread_t th(slice_->home_thread);
    if (master_) coro_t::spawn_on_thread(master_->home_thread, boost::bind(&master_t::incr_decr, master_.get(), kind, key, amount, castime));
    return slice_->incr_decr(kind, key, amount, castime);
}

append_prepend_result_t btree_slice_dispatching_to_master_t::append_prepend(append_prepend_kind_t kind, const store_key_t &key, data_provider_t *data, castime_t castime) {
    on_thread_t th(slice_->home_thread);
    if (master_) {
        buffer_borrowing_data_provider_t borrower(master_->home_thread, data);
        coro_t::spawn_on_thread(master_->home_thread, boost::bind(&master_t::append_prepend, master_.get(), kind, key, borrower.side_provider(), castime));
        return slice_->append_prepend(kind, key, &borrower, castime);
    } else {
        return slice_->append_prepend(kind, key, data, castime);
    }
}

delete_result_t btree_slice_dispatching_to_master_t::delete_key(const store_key_t &key, repli_timestamp timestamp) {
    on_thread_t th(slice_->home_thread);
    if (master_) coro_t::spawn_on_thread(master_->home_thread, boost::bind(&master_t::delete_key, master_.get(), key, timestamp));
    return slice_->delete_key(key, timestamp);
}

void btree_slice_dispatching_to_master_t::nop_back_on_masters_thread(repli_timestamp timestamp, cond_t *cond, int *counter) {
    debugf("thread id is %d after spawn\n", get_thread_id());
    rassert(get_thread_id() == master_->home_thread);

    repli_timestamp t;
    {
        on_thread_t th(slice_->home_thread);

        t = current_time();
        rassert(t.time >= timestamp.time);
    }

    --*counter;
    rassert(*counter >= 0);
    if (*counter == 0) {
        cond->pulse();
    }
}
