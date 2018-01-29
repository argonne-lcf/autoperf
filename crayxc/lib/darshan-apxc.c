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
#include <papi.h>

#include "uthash.h"
#include "darshan.h"
#include "darshan-dynamic.h"
#include "darshan-apxc-log-format.h"

/*
 * PAPI_events are defined by the Aries counters listed in the log header.
 */
#define QUOTE(a) #a
#define X(a) QUOTE(a),
static char* PAPI_events[] =
{
    APXC_RTR_COUNTERS
};
#undef X
#undef QUOTE

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
    struct darshan_apxc_router_record *rtr_record;
    darshan_record_id header_id;
    darshan_record_id rtr_id;
    int PAPI_event_set;
    int PAPI_event_count;
    int group;
    int chassis;
    int blade;
    int node;
};

static struct apxc_runtime *apxc_runtime = NULL;
static pthread_mutex_t apxc_runtime_mutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;

/* my_rank indicates the MPI rank of this process */
static int my_rank = -1;

/* internal helper functions for the APXC module */
void apxc_runtime_initialize(void);

/* forward declaration for shutdown function needed to interface with darshan-core */
static void apxc_shutdown(MPI_Comm mod_comm, darshan_record_id *shared_recs, int shared_rec_count, void **buffer, int *size);

/* macros for obtaining/releasing the APXC module lock */
#define APXC_LOCK() pthread_mutex_lock(&apxc_runtime_mutex)
#define APXC_UNLOCK() pthread_mutex_unlock(&apxc_runtime_mutex)

/*
 * Initialize counters using PAPI
 */
static void initialize_counters (void)
{
    int i = 0;
    int code = 0;

    PAPI_library_init(PAPI_VER_CURRENT);
    PAPI_create_eventset(&apxc_runtime->PAPI_event_set);

    while(strcmp(PAPI_events[i], "APXC_RTR_NUM_INDICES") != 0)
    {
        PAPI_event_name_to_code(PAPI_events[i], &code);
        PAPI_add_event(apxc_runtime->PAPI_event_set, code);
        i++;
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

static void get_coords (void)
{
    FILE *f = fopen("/proc/cray_xt/cname","r");

    if (f != NULL)
    {
        char a, b, c, d;
        int racki, rackj, chassis, blade, nic;

        /* format example: c1-0c1s2n1 c3-0c2s15n3 */
        fscanf(f, "%c%d-%d%c%d%c%d%c%d", 
           &a, &racki, &rackj, &b, &chassis, &c, &blade, &d, &nic);

        fclose(f);

        apxc_runtime->group   = racki/2 + rackj*6;
        apxc_runtime->chassis = (racki%2) * 3 + chassis;
        apxc_runtime->blade   = blade;
        apxc_runtime->node    = nic;
    }

    return;
}

/*
 * Function which updates all the counter data
 */
static void capture(struct darshan_apxc_router_record *rec,
                    darshan_record_id rec_id)
{
    FILE *f;

    PAPI_stop(apxc_runtime->PAPI_event_set, (long long*) rec->counters);
    PAPI_reset(apxc_runtime->PAPI_event_set);

    rec->coord[0] = apxc_runtime->group;
    rec->coord[1] = apxc_runtime->chassis;
    rec->coord[2] = apxc_runtime->blade;
    rec->coord[3] = apxc_runtime->node;

    rec->base_rec.id = rec_id;
    rec->base_rec.rank = my_rank;

    return;
}

void apxc_runtime_initialize()
{
    int apxc_buf_size;
    char rtr_rec_name[32];

    APXC_LOCK();

    /* don't do anything if already initialized */
    if(apxc_runtime)
    {
        APXC_UNLOCK();
        return;
    }

    apxc_buf_size = sizeof(struct darshan_apxc_header_record) + 
                    sizeof(struct darshan_apxc_router_record);

    /* register the BG/Q module with the darshan-core component */
    darshan_core_register_module(
        DARSHAN_APXC_MOD,
        &apxc_shutdown,
        &apxc_buf_size,
        &my_rank,
        NULL);

    /* not enough memory to fit crayxc module record */
    if(apxc_buf_size < sizeof(struct darshan_apxc_header_record) + sizeof(struct darshan_apxc_router_record))
    {
        darshan_core_unregister_module(DARSHAN_APXC_MOD);
        APXC_UNLOCK();
        return;
    }

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
        apxc_runtime->header_id = darshan_core_gen_record_id("darshan-crayxc-header");

        /* register the crayxc file record with darshan-core */
        apxc_runtime->header_record = darshan_core_register_record(
            apxc_runtime->header_id,
            NULL,
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
    }

    get_coords();

    sprintf(rtr_rec_name, "darshan-crayxc-rtr-%d%d%d",
            apxc_runtime->group, apxc_runtime->chassis, apxc_runtime->blade);

    apxc_runtime->rtr_id = darshan_core_gen_record_id(rtr_rec_name);

    apxc_runtime->rtr_record = darshan_core_register_record(
        apxc_runtime->rtr_id,
        NULL,
        DARSHAN_APXC_MOD,
        sizeof(struct darshan_apxc_router_record),
        NULL);
    if(!(apxc_runtime->rtr_record))
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

/* Pass data for the crayxc module back to darshan-core to log to file. */
static void apxc_shutdown(
    MPI_Comm mod_comm,
    darshan_record_id *shared_recs,
    int shared_rec_count,
    void **buffer,
    int *size)
{
    int result;
    int i;
    int color;
    int router_rank;
    int router_count;
    int chassis_count;
    int group_count;
    MPI_Comm router_comm;
    MPI_Comm chassis_comm;
    MPI_Comm group_comm;

    APXC_LOCK();

    if (!apxc_runtime)
    {
        APXC_UNLOCK();
        return;
    }

    /* collect data */
    capture(apxc_runtime->rtr_record, apxc_runtime->rtr_id);

    /*
     * reduce data
     *
     *  aggregate data from processes which share the same blade and avg.
     *  
     */ 
    color = (apxc_runtime->group << 5) + \
            (apxc_runtime->chassis << 2) + \
            apxc_runtime->blade;
    PMPI_Comm_split(MPI_COMM_WORLD, color, my_rank, &router_comm);
    PMPI_Comm_split(MPI_COMM_WORLD, (color & ~(0x3)), my_rank, &chassis_comm);
    PMPI_Comm_split(MPI_COMM_WORLD, (color & ~(0x1f)), my_rank, &group_comm);

    PMPI_Comm_size(chassis_comm, &chassis_count);
    PMPI_Comm_size(group_comm,   &group_count);
    PMPI_Comm_rank(router_comm,  &router_rank);
    PMPI_Comm_size(router_comm,  &router_count);

    PMPI_Reduce(apxc_runtime->rtr_record->counters,
                apxc_runtime->rtr_record->counters,
                apxc_runtime->PAPI_event_count,
                MPI_LONG_LONG_INT,
                MPI_SUM,
                0,
                router_comm);
    if (router_rank == 0)
    {
        for (i = 0; i < apxc_runtime->PAPI_event_count; i++)
        {
            apxc_runtime->rtr_record->counters[i] /= router_count;
        }
    }
    else
    {
        /* discard other ranks non-unique router blades */
        *size -= sizeof(*apxc_runtime->rtr_record);
    }

    if (my_rank == 0)
    {
        apxc_runtime->header_record->nblades  = router_count;
        apxc_runtime->header_record->nchassis = chassis_count;
        apxc_runtime->header_record->ngroups  = group_count;
    }

    PMPI_Comm_free(&router_comm);
    PMPI_Comm_free(&chassis_comm);
    PMPI_Comm_free(&group_comm);

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
