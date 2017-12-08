/*
 * Copyright (C) 2017 University of Chicago.
 * See COPYRIGHT notice in top-level directory.
 *
 */

#define _XOPEN_SOURCE 500
#define _GNU_SOURCE

#include "darshan-runtime-config.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>

#include "uthash.h"
#include "darshan.h"
#include "darshan-dynamic.h"

/*
 * <Description>
 * 
 * This module does not intercept any system calls. It just pulls data
 * from the personality struct at initialization.
 */


/*
 * Global runtime struct for tracking data needed at runtime
 */
struct crayxc_runtime
{
    struct darshan_crayxc_record *record;
};

static struct crayxc_runtime *crayxc_runtime = NULL;
static pthread_mutex_t crayxc_runtime_mutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;

/* my_rank indicates the MPI rank of this process */
static int my_rank = -1;

/* internal helper functions for the BGQ module */
void crayxc_runtime_initialize(void);

/* forward declaration for shutdown function needed to interface with darshan-core */
static void crayxc_shutdown(MPI_Comm mod_comm, darshan_record_id *shared_recs, int shared_rec_count, void **buffer, int *size);

/* macros for obtaining/releasing the BGQ module lock */
#define CRAYXC_LOCK() pthread_mutex_lock(&crayxc_runtime_mutex)
#define CRAYXC_UNLOCK() pthread_mutex_unlock(&crayxc_runtime_mutex)

/*
 * Function which updates all the counter data
 */
static void capture(struct darshan_crayxc_record *rec, darshan_record_id rec_id)
{
    rec->counters[XXXX] = val;

    rec->base_rec.id = rec_id;
    rec->base_rec.rank = my_rank;
    rec->fcounters[XXX_F_TIMESTAMP] = darshan_core_wtime();

    return;
}

/**********************************************************
 * Internal functions for manipulating BGQ module state *
 **********************************************************/

void crayxc_runtime_initialize()
{
    int crayxc_buf_size;
    darshan_record_id rec_id;

    CRAYXC_LOCK();

    /* don't do anything if already initialized */
    if(crayxc_runtime)
    {
        CRAYXC_UNLOCK();
        return;
    }

    /* we just need to store one single record */
    crayxc_buf_size = sizeof(struct darshan_crayxc_record);

    /* register the BG/Q module with the darshan-core component */
    darshan_core_register_module(
        DARSHAN_CRAYXC_MOD,
        &crayxc_shutdown,
        &crayxc_buf_size,
        &my_rank,
        NULL);

    /* not enough memory to fit crayxc module record */
    if(crayxc_buf_size < sizeof(struct darshan_crayxc_record))
    {
        darshan_core_unregister_module(DARSHAN_CRAYXC_MOD);
        CRAYXC_UNLOCK();
        return;
    }

    /* initialize module's global state */
    crayxc_runtime = malloc(sizeof(*crayxc_runtime));
    if(!crayxc_runtime)
    {
        darshan_core_unregister_module(DARSHAN_CRAYXC_MOD);
        CRAYXC_UNLOCK();
        return;
    }
    memset(crayxc_runtime, 0, sizeof(*crayxc_runtime));

    rec_id = darshan_core_gen_record_id("darshan-crayxc-record");

    /* register the crayxc file record with darshan-core */
    crayxc_runtime->record = darshan_core_register_record(
        rec_id,
        NULL,
        DARSHAN_CRAYXC_MOD,
        sizeof(struct darshan_crayxc_record),
        NULL);
    if(!(crayxc_runtime->record))
    {
        darshan_core_unregister_module(DARSHAN_CRAYXC_MOD);
        free(crayxc_runtime);
        crayxc_runtime = NULL;
        CRAYXC_UNLOCK();
        return;
    }

    capture(crayxc_runtime->record, rec_id);

    CRAYXC_UNLOCK();

    return;
}

/********************************************************************************
 * shutdown function exported by this module for coordinating with darshan-core *
 ********************************************************************************/

/* Pass data for the crayxc module back to darshan-core to log to file. */
static void crayxc_shutdown(
    MPI_Comm mod_comm,
    darshan_record_id *shared_recs,
    int shared_rec_count,
    void **buffer,
    int *size)
{
    int nprocs;
    int result;
    uint64_t *ion_ids;

    CRAYXC_LOCK();
    assert(crayxc_runtime);

    /* non-zero ranks throw out their CRAYXC record */
    if (my_rank != 0)
    {
        *buffer = NULL;
        *size   = 0;
    }

    free(crayxc_runtime);
    crayxc_runtime = NULL;

    CRAYXC_UNLOCK();

    return;
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 * End:
 *
 * vim: ts=8 sts=4 sw=4 expandtab
 */
