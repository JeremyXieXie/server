// T(A) gets W(L)
// T(B) tries R(L), gets DB_LOCK_NOTGRANTED
// T(C) tries R(L), gets DB_LOCK_NOTGRANTED
// T(A) releases locks
// T(B) gets R(L)
// T(C) still pending (since we only have 1 lock
// T(B) releases lock
// T(C) gets R(L)
// T(C) releases locks

#include "test.h"

int main(int argc, const char *argv[]) {
    int r;

    uint32_t max_locks = 1;
    uint64_t max_lock_memory = 4096;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose++;
            continue;
        }
        if (strcmp(argv[i], "-q") == 0 || strcmp(argv[i], "--quiet") == 0) {
            if (verbose > 0) verbose--;
            continue;
        }
        if (strcmp(argv[i], "--max_locks") == 0 && i+1 < argc) {
            max_locks = atoi(argv[++i]);
            continue;
        }
        if (strcmp(argv[i], "--max_lock_memory") == 0 && i+1 < argc) {
            max_lock_memory = atoi(argv[++i]);
            continue;
        }        
        assert(0);
    }

    // setup
    toku_ltm *ltm = NULL;
    r = toku_ltm_create(&ltm, max_locks, max_lock_memory, dbpanic, get_compare_fun_from_db);
    assert(r == 0 && ltm);

    toku_lock_tree *lt = NULL;
    r = toku_lt_create(&lt, dbpanic, ltm, get_compare_fun_from_db);
    assert(r == 0 && lt);

    const TXNID txn_a = 1;
    DBT key_l; dbt_init(&key_l, "L", 1);
    toku_lock_request a_w_l; toku_lock_request_init(&a_w_l, (DB *)1, txn_a, &key_l, &key_l, LOCK_REQUEST_WRITE);
    r = toku_lock_request_start(&a_w_l, lt, false); assert(r == 0); 
    assert(a_w_l.state == LOCK_REQUEST_COMPLETE && a_w_l.complete_r == 0);
    toku_lock_request_destroy(&a_w_l);

    const TXNID txn_b = 2;
    toku_lock_request b_w_l; toku_lock_request_init(&b_w_l, (DB *)1, txn_b, &key_l, &key_l, LOCK_REQUEST_READ);
    r = toku_lock_request_start(&b_w_l, lt, false); assert(r != 0); 
    assert(b_w_l.state == LOCK_REQUEST_PENDING);

    const TXNID txn_c = 3;
    toku_lock_request c_w_l; toku_lock_request_init(&c_w_l, (DB *)1, txn_c, &key_l, &key_l, LOCK_REQUEST_READ);
    r = toku_lock_request_start(&c_w_l, lt, false); assert(r != 0); 
    assert(c_w_l.state == LOCK_REQUEST_PENDING);

    r = toku_lt_unlock(lt, txn_a); assert(r == 0);
    assert(b_w_l.state == LOCK_REQUEST_COMPLETE && b_w_l.complete_r == 0);
    assert(c_w_l.state == LOCK_REQUEST_COMPLETE && c_w_l.complete_r == TOKUDB_OUT_OF_LOCKS);
    toku_lock_request_destroy(&b_w_l);
    r = toku_lt_unlock(lt, txn_b);  assert(r == 0);
    toku_lock_request_destroy(&c_w_l);
    r = toku_lt_unlock(lt, txn_c);  assert(r == 0);

    // shutdown 
    r = toku_lt_close(lt); assert(r == 0);
    r = toku_ltm_close(ltm); assert(r == 0);

    return 0;
}
