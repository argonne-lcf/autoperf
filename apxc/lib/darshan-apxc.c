/*
 * Copyright (C) 2017 University of Chicago.
 * See COPYRIGHT notice in top-level directory.
 *
 */

#define _XOPEN_SOURCE 500
#define _GNU_SOURCE
#define csJOBID_ENV_STR "ALPS_APP_ID"

#include "darshan-runtime-config.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>
#include <papi.h>

#include "uthash.h"
#include "darshan.h"
#include "darshan-dynamic.h"
#include "darshan-apxc-log-format.h"

#include "darshan-apxc-utils.h"

/*
 * PAPI_events are defined by the Aries counters listed in the log header.
 */
#define X(a) #a,
#define Z(a) #a
static char* PAPI_events[] =
{
    APXC_PERF_COUNTERS
};
#undef X
#undef Z

#define MAX_GROUPS (128)
#define MAX_CHASSIS (MAX_GROUPS*6)
#define MAX_BLADES (MAX_CHASSIS*16)

/*
 * <Description>
 * 
 * This module does not intercept any system calls. It just pulls data
 * from the personality struct at initialization.
 */


/*
 * Global runtime struct for tracking data needed at runtime
 */
struct apxc_runtime
{
    struct darshan_apxc_header_record *header_record;
    struct darshan_apxc_perf_record *perf_record;
    darshan_record_id header_id;
    darshan_record_id rtr_id;
    int PAPI_event_set;
    int PAPI_event_count;
    int group;
    int chassis;
    int blade;
    int node;
    int perf_record_marked;
};

static struct apxc_runtime *apxc_runtime = NULL;
static pthread_mutex_t apxc_runtime_mutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;

/* my_rank indicates the MPI rank of this process */
static int my_rank = -1;

/* internal helper functions for the APXC module */
void apxc_runtime_initialize(void);

/* forward declaration for shutdown function needed to interface with darshan-core */
//#ifdef HAVE_MPI
static void apxc_mpi_redux(
    void *buffer, 
    MPI_Comm mod_comm,
    darshan_record_id *shared_recs, 
    int shared_rec_count);
//#endif
static void apxc_shutdown(
        void **buffer, 
        int *size);

/* macros for obtaining/releasing the APXC module lock */
#define APXC_LOCK() pthread_mutex_lock(&apxc_runtime_mutex)
#define APXC_UNLOCK() pthread_mutex_unlock(&apxc_runtime_mutex)

/*
 * Initialize counters using PAPI
 */
static void initialize_counters (void)
{
    int i;
    int code = 0;

    PAPI_library_init(PAPI_VER_CURRENT);
    apxc_runtime->PAPI_event_set = PAPI_NULL;
    PAPI_create_eventset(&apxc_runtime->PAPI_event_set);

    /* start with first PAPI counter */
    for (i = AR_RTR_0_0_INQ_PRF_INCOMING_FLIT_VC0;
         strcmp(PAPI_events[i], "APXC_NUM_INDICES") != 0;
         i++)
    {
        PAPI_event_name_to_code(PAPI_events[i], &code);
        PAPI_add_event(apxc_runtime->PAPI_event_set, code);
    }

    apxc_runtime->PAPI_event_count = i;

    PAPI_start(apxc_runtime->PAPI_event_set);

    return;
}

static void finalize_counters (void)
{
    PAPI_cleanup_eventset(apxc_runtime->PAPI_event_set);
    PAPI_destroy_eventset(&apxc_runtime->PAPI_event_set);
    PAPI_shutdown();

    return;
}

/*
 * Function which updates all the counter data
 */
static void capture(struct darshan_apxc_perf_record *rec,
                    darshan_record_id rec_id)
{
    PAPI_stop(apxc_runtime->PAPI_event_set,
          (long long*) &rec->counters[AR_RTR_0_0_INQ_PRF_INCOMING_FLIT_VC0]);
    PAPI_reset(apxc_runtime->PAPI_event_set);

    rec->group   = apxc_runtime->group;
    rec->chassis = apxc_runtime->chassis;
    rec->blade   = apxc_runtime->blade;
    rec->node    = apxc_runtime->node;
    rec->base_rec.id = rec_id;
    rec->base_rec.rank = my_rank;

    return;
}

void apxc_runtime_initialize()
{
    size_t apxc_buf_size;
    char rtr_rec_name[128];

    darshan_module_funcs mod_funcs = {
//#ifdef HAVE_MPI
        .mod_redux_func = &apxc_mpi_redux,
//#endif
        .mod_shutdown_func = &apxc_shutdown
        };

    APXC_LOCK();
    

    /* don't do anything if already initialized */
    if(apxc_runtime)
    {
        APXC_UNLOCK();
        return;
    }


    apxc_buf_size = sizeof(struct darshan_apxc_header_record) + 
                    sizeof(struct darshan_apxc_perf_record);

    /* register the APXC module with the darshan-core component */
    darshan_core_register_module(
        DARSHAN_APXC_MOD,
        mod_funcs,
        &apxc_buf_size,
        &my_rank,
        NULL);


    /* initialize module's global state */
    apxc_runtime = malloc(sizeof(*apxc_runtime));
    if(!apxc_runtime)
    {
        darshan_core_unregister_module(DARSHAN_APXC_MOD);
        APXC_UNLOCK();
        return;
    }
    memset(apxc_runtime, 0, sizeof(*apxc_runtime));

    if (my_rank == 0)
    {
        apxc_runtime->header_id = darshan_core_gen_record_id("darshan-apxc-header");

        /* register the apxc file record with darshan-core */
        apxc_runtime->header_record = darshan_core_register_record(
            apxc_runtime->header_id,
            //NULL,
            "darshan-apxc-header",
            DARSHAN_APXC_MOD,
            sizeof(struct darshan_apxc_header_record),
            NULL);
        if(!(apxc_runtime->header_record))
        {
            darshan_core_unregister_module(DARSHAN_APXC_MOD);
            free(apxc_runtime);
            apxc_runtime = NULL;
            APXC_UNLOCK();
           return;
        }
        apxc_runtime->header_record->base_rec.id = apxc_runtime->header_id;
        apxc_runtime->header_record->base_rec.rank = my_rank;
        apxc_runtime->header_record->magic = APXC_MAGIC;
    }

    get_xc_coords(&apxc_runtime->group,
                  &apxc_runtime->chassis,
                  &apxc_runtime->blade,
                  &apxc_runtime->node);

    sprintf(rtr_rec_name, "darshan-apxc-rtr-%d-%d-%d",
            apxc_runtime->group, apxc_runtime->chassis, apxc_runtime->blade);
    //apxc_runtime->rtr_id = darshan_core_gen_record_id(rtr_rec_name);
    apxc_runtime->rtr_id = darshan_core_gen_record_id("APXC");
    apxc_runtime->perf_record = darshan_core_register_record(
        apxc_runtime->rtr_id,
        //NULL,
        "APXC",   // we want the record for each rank to be treated as shared records so that mpi_redux can operate on
        //rtr_rec_name,
        DARSHAN_APXC_MOD,
        sizeof(struct darshan_apxc_perf_record),
        NULL);
    if(!(apxc_runtime->perf_record))
    {
        darshan_core_unregister_module(DARSHAN_APXC_MOD);
        free(apxc_runtime);
        apxc_runtime = NULL;
        APXC_UNLOCK();
        return;
    }

    initialize_counters();
    APXC_UNLOCK();

    return;
}

/********************************************************************************
 * shutdown function exported by this module for coordinating with darshan-core *
 ********************************************************************************/

/* Pass data for the apxc module back to darshan-core to log to file. */
//#ifdef HAVE_MPI
static void apxc_mpi_redux(
    void *apxc_buf,
    MPI_Comm mod_comm,
    darshan_record_id *shared_recs,
    int shared_rec_count)
{
    int i;
    int color;
    int router_rank;
    int router_count;
    int chassis_count;
    int group_count;
    int mmode, rmmode;
    int cmode, rcmode;
    unsigned int *bitvec;
    unsigned int bitlen;
    unsigned int bitcnt;
    unsigned int bitsiz;
    MPI_Comm router_comm;

    APXC_LOCK();
    if (!apxc_runtime)
    {
        APXC_UNLOCK();
        return;
    }

    bitcnt = sizeof(unsigned int) * 8;
    bitlen = sizeof(unsigned int) * (MAX_BLADES/bitcnt);
    bitsiz = bitlen / sizeof(unsigned int);
    bitvec = malloc(bitlen);
    
    /* collect perf counters */
    capture(apxc_runtime->perf_record, apxc_runtime->rtr_id);

    /* collect memory/cluster config */
    mmode = get_memory_mode(apxc_runtime->node);
    cmode = get_cluster_mode(apxc_runtime->node);

    PMPI_Reduce(&mmode,
                &rmmode, 1, MPI_INT, MPI_BOR, 0, MPI_COMM_WORLD);
    PMPI_Reduce(&cmode,
                &rcmode, 1, MPI_INT, MPI_BOR, 0, MPI_COMM_WORLD);

    if (my_rank == 0)
    {
        apxc_runtime->header_record->memory_mode = mmode;
        apxc_runtime->header_record->cluster_mode = cmode;
        if (mmode != rmmode)
            apxc_runtime->header_record->memory_mode |= (1 << 31);
        if (cmode != rcmode)
            apxc_runtime->header_record->cluster_mode |= (1 << 31);
        apxc_runtime->header_record->appid = atoi((char*)getenv( csJOBID_ENV_STR ));
    }

    /* count network dimensions */
    if (bitvec)
    {
        int idx;
        unsigned int uchassis;
        unsigned int ublade;

        /* group */
        memset(bitvec, 0, bitlen);
        idx = apxc_runtime->group / bitcnt;
        bitvec[idx] |= (1 << apxc_runtime->group % bitcnt);
        PMPI_Reduce((my_rank ? bitvec : MPI_IN_PLACE),
                     bitvec,
                     bitsiz,
                     MPI_INT,
                     MPI_BOR,
                     0,
                     MPI_COMM_WORLD);
        group_count = count_bits(bitvec, bitsiz);

        /* chassis */
        memset(bitvec, 0, bitlen);
        uchassis = apxc_runtime->group * 6 + apxc_runtime->chassis;
        idx = uchassis / bitcnt;
        bitvec[idx] |= (1 << uchassis % bitcnt);
        PMPI_Reduce((my_rank ? bitvec : MPI_IN_PLACE),
                    bitvec,
                    bitsiz,
                    MPI_INT,
                    MPI_BOR,
                    0,
                    MPI_COMM_WORLD);
        chassis_count = count_bits(bitvec, bitsiz);

        /* blade */
        memset(bitvec, 0, bitlen);
        ublade = uchassis * 16 + apxc_runtime->blade;
        idx = ublade / bitcnt;
        bitvec[idx] |= (1 << ublade % bitcnt);
        PMPI_Reduce((my_rank ? bitvec : MPI_IN_PLACE),
                    bitvec,
                    bitsiz,
                    MPI_INT,
                    MPI_BOR,
                    0,
                    MPI_COMM_WORLD);
        router_count = count_bits(bitvec, bitsiz);

        if (my_rank == 0)
        {
            apxc_runtime->header_record->nblades  = router_count;
            apxc_runtime->header_record->nchassis = chassis_count;
            apxc_runtime->header_record->ngroups  = group_count;
        }
        free(bitvec);
    }
    else
    {
        apxc_runtime->header_record->nblades  = 0;
        apxc_runtime->header_record->nchassis = 0;
        apxc_runtime->header_record->ngroups  = 0;
    }
    
    /*
     * reduce data
     *
     *  aggregate data from processes which share the same blade and avg.
     *  
     */ 
    color = (apxc_runtime->group << (4+3)) + \
            (apxc_runtime->chassis << 4) + \
            apxc_runtime->blade;
    PMPI_Comm_split(MPI_COMM_WORLD, color, my_rank, &router_comm);
    PMPI_Comm_rank(router_comm,  &router_rank);
    PMPI_Comm_size(router_comm,  &router_count);

    PMPI_Reduce((router_rank?apxc_runtime->perf_record->counters:MPI_IN_PLACE),
                apxc_runtime->perf_record->counters,
                APXC_NUM_INDICES,
                MPI_LONG_LONG_INT,
                MPI_SUM,
                0,
                router_comm);

    if (router_rank == 0)
    {
        for (i = 0; i < APXC_NUM_INDICES; i++)
        {
            apxc_runtime->perf_record->counters[i] /= router_count;
        }
            apxc_runtime->perf_record_marked = -1;
    }
    PMPI_Comm_free(&router_comm);

    APXC_UNLOCK();

    return;
}

//#endif
static void apxc_shutdown(
    void **apxc_buf,
    int *apxc_buf_sz)
{
    APXC_LOCK();
    assert(apxc_runtime);
    *apxc_buf_sz = 0; 
    
    if (my_rank == 0) { 
        *apxc_buf_sz += sizeof(*apxc_runtime->header_record); 
    }
    
    if (apxc_runtime->perf_record_marked == -1) 
     { 
       *apxc_buf_sz += sizeof( *apxc_runtime->perf_record); 
     }
    finalize_counters();
    free(apxc_runtime);
    apxc_runtime = NULL;

    APXC_UNLOCK();
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
