/*
 * Copyright (C) 2017 University of Chicago.
 * See COPYRIGHT notice in top-level directory.
 *
 */

#define _XOPEN_SOURCE 500
#define _GNU_SOURCE

/*
 * TODO: A job id given by the job scheduler uniquely identifies a job at a system level,
 *       similarly, the application launcher id uniquely identifies an application
 *       execution on the system.
 */
//#ifdef SLURM_JOBID
#define APP_ID "SLURM_TASK_PID"
//#endif
//#ifdef PBS_JOBID
//#define APP_ID "PALS_APP_ID
//#endif

#include "darshan-runtime-config.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <dirent.h>
#include <assert.h>

#include "uthash.h"
#include "darshan.h"
#include "darshan-dynamic.h"
#include "darshan-apcxi-log-format.h"

#include "darshan-apcxi-utils.h"
#include "crusher_nodelist.h"
#define MAX_COUNTER_NAME_LENGTH 100

/*
 * apcxi_events are defined by the Cassini counters listed in the log header.
 */
#define X(a) #a,
#define Z(a) #a
static char* apcxi_events[] =
{
    APCXI_PERF_COUNTERS
};
#undef X
#undef Z

char ***apcxi_filenames;

#define MAX_GROUPS (128)
#define MAX_CHASSIS (MAX_GROUPS*8)
#define MAX_SLOTS (MAX_CHASSIS*8)
#define MAX_BLADES (MAX_SLOTS*2)

/*
 * <Description>
 * 
 * This module does not intercept any system calls. It just pulls data
 * from the personality struct at initialization.
 */


/*
 * Global runtime struct for tracking data needed at runtime
 */
struct apcxi_runtime
{
    struct darshan_apcxi_header_record *header_record;
    struct darshan_apcxi_perf_record *perf_record;
    darshan_record_id header_id;
    darshan_record_id rtr_id;
    int num_counters;
    int group;
    int chassis;
    int slot;
    int blade;
    int node;
    int num_nics;
    int perf_record_marked;
};

static struct apcxi_runtime *apcxi_runtime = NULL;
static pthread_mutex_t apcxi_runtime_mutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;

/* my_rank indicates the MPI rank of this process */
static int my_rank = -1;

/* internal helper functions for the APCXI module */
void apcxi_runtime_initialize(void);

/* forward declaration for shutdown function needed to interface with darshan-core */
//#ifdef HAVE_MPI
static void apcxi_mpi_redux(
        void *buffer, 
        MPI_Comm mod_comm,
        darshan_record_id *shared_recs, 
        int shared_rec_count);
//#endif
static void apcxi_output(
        void **buffer, 
        int *size);
static void apcxi_cleanup(
        void);

/* macros for obtaining/releasing the APCXI module lock */
#define APCXI_LOCK() pthread_mutex_lock(&apcxi_runtime_mutex)
#define APCXI_UNLOCK() pthread_mutex_unlock(&apcxi_runtime_mutex)

/*
 * Initialize counters
 * determine the number of nic devices on the node
 * build the list of files to read from the sysfs telemetry
 */
static void initialize_counters (void)
{
    int i;
    int code = 0;


    /* start with first apcxi counter */
    i = CQ_CQ_OXE_NUM_STALLS;
    while(strcmp(apcxi_events[i], "APCXI_NUM_INDICES") != 0){
        i++;
    }
    apcxi_runtime->num_counters = i;

    // allocate memory space for the sys fs file names
    // for each of the available nics (num_nics)
    apcxi_filenames = (char***) malloc(sizeof(char**) * apcxi_runtime->num_nics);
    for(int i=0; i<apcxi_runtime->num_nics; i++){
        apcxi_filenames[i] = (char**) malloc(sizeof(char*) * apcxi_runtime->num_counters);
        for(int j=0; j<apcxi_runtime->num_counters; j++){
            apcxi_filenames[i][j] = (char*) malloc(sizeof(char) * MAX_COUNTER_NAME_LENGTH);
        }
    }

    char line[MAX_COUNTER_NAME_LENGTH];
    for(int i=0; i<apcxi_runtime->num_counters; i++)
    {
        size_t ln = strlen(apcxi_events[i]);
        strcpy(line, apcxi_events[i]);
        strlwr(line);
        line[ln] = '\0';
        for(int j=0; j<apcxi_runtime->num_nics; j++){
            // /sys/class/cxi/cxi[0..3]
            char str[MAX_COUNTER_NAME_LENGTH] = "";
            char buf[5];
            snprintf(buf, sizeof(buf), "%d", j);
            strcat(str, "/sys/class/cxi/cxi");
            strcat(str, buf);
            strcat(str, "/device/telemetry/");
            strcat(str, line);
            strcpy(apcxi_filenames[j][i],str);
        }
    }

    FILE *fff;
    for(int i=0; i<apcxi_runtime->num_nics; i++){
        for(int j=0; j<apcxi_runtime->num_counters; j++){
            fff = fopen(apcxi_filenames[i][j], "r");
            if (fff == NULL) {
                fprintf(stderr,"\tError opening %s!\n",apcxi_filenames[i][j]);
            }
            else {
                uint64_t b, c;
                fscanf(fff,"%lu@%lu.%lu",&apcxi_runtime->perf_record->counters[i][j], &b, &c);
                fclose(fff);
            }
        }
    }
    return;
}

static void finalize_counters (void)
{
    // free apcxi_filenames
    for(int i=0; i<apcxi_runtime->num_nics; i++){
        for(int j=0; j<apcxi_runtime->num_counters; j++){
            free(apcxi_filenames[i][j]);
        }
        free(apcxi_filenames[i]);
    }
    free(apcxi_filenames);
    return;
}

/*
 * Function which updates all the counter data
 */
static void capture(struct darshan_apcxi_perf_record *rec,
        darshan_record_id rec_id)
{
    FILE *fff;
    for(int i=0; i<apcxi_runtime->num_nics; i++){
        for(int j=0; j<apcxi_runtime->num_counters; j++){
            fff = fopen(apcxi_filenames[i][j], "r");
            if (fff == NULL) {
                fprintf(stderr,"\tError opening %s!\n",apcxi_filenames[i][j]);
            }
            else {
                uint64_t a, b, c;
                fscanf(fff,"%lu@%lu.%lu", &a, &b, &c);
                apcxi_runtime->perf_record->counters[i][j] = a - apcxi_runtime->perf_record->counters[i][j];
                fclose(fff);
            }
        }
    }

    rec->group   = apcxi_runtime->group;
    rec->chassis = apcxi_runtime->chassis;
    rec->slot = apcxi_runtime->slot;
    rec->blade   = apcxi_runtime->blade;
    rec->node    = apcxi_runtime->node;
    rec->num_nics    = apcxi_runtime->num_nics;
    rec->base_rec.id = rec_id;
    rec->base_rec.rank = my_rank;

    return;
}

void apcxi_runtime_initialize()
{
    size_t apcxi_buf_size;
    char rtr_rec_name[128];
    darshan_module_funcs mod_funcs = {
        //#ifdef HAVE_MPI
        .mod_redux_func = &apcxi_mpi_redux,
        //#endif
        .mod_output_func = &apcxi_output,
        .mod_cleanup_func = &apcxi_cleanup
    };

    APCXI_LOCK();

    /*
     *  Determine the number of available NIC devices on the compute node
     *  read the number of dirs in /sys/class/cxi/
     */
    int num_nics = 0;
    struct dirent *dp;
    DIR *fdir;
    if ((fdir = opendir("/sys/class/cxi")) == NULL) {
        fprintf(stderr, "listdir: can't open %s\n", "/sys/class/cxi");
        return;
    }
    while ((dp = readdir(fdir)) != NULL) {
        if (!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, ".."))
            continue;    /* skip self and parent */
        num_nics++;
    }
    closedir(fdir);


    /* don't do anything if already initialized */
    if(apcxi_runtime)
    {
        APCXI_UNLOCK();
        return;
    }


    apcxi_buf_size = sizeof(struct darshan_apcxi_header_record) + 
        APCXI_PERF_RECORD_SIZE(num_nics);

    /* register the APCXI module with the darshan-core component */
    darshan_core_register_module(
            DARSHAN_APCXI_MOD,
            mod_funcs,
            &apcxi_buf_size,
            &my_rank,
            NULL);

    /* initialize module's global state */
    apcxi_runtime = malloc(sizeof(*apcxi_runtime));
    if(!apcxi_runtime)
    {
        darshan_core_unregister_module(DARSHAN_APCXI_MOD);
        APCXI_UNLOCK();
        return;
    }
    memset(apcxi_runtime, 0, sizeof(*apcxi_runtime));
    apcxi_runtime->num_nics = num_nics;

    if (my_rank == 0)
    {
        apcxi_runtime->header_id = darshan_core_gen_record_id("darshan-apcxi-header");

        /* register the apcxi file record with darshan-core */
        apcxi_runtime->header_record = darshan_core_register_record(
                apcxi_runtime->header_id,
                //NULL,
                "darshan-apcxi-header",
                DARSHAN_APCXI_MOD,
                sizeof(struct darshan_apcxi_header_record),
                NULL);
        if(!(apcxi_runtime->header_record))
        {
            darshan_core_unregister_module(DARSHAN_APCXI_MOD);
            free(apcxi_runtime);
            apcxi_runtime = NULL;
            APCXI_UNLOCK();
            return;
        }
        apcxi_runtime->header_record->base_rec.id = apcxi_runtime->header_id;
        apcxi_runtime->header_record->base_rec.rank = my_rank;
        apcxi_runtime->header_record->magic = APCXI_MAGIC;
    }
    sstopo_get_mycoords(&apcxi_runtime->group,
            &apcxi_runtime->chassis,
            &apcxi_runtime->slot,
            &apcxi_runtime->blade,
            &apcxi_runtime->node);
    sprintf(rtr_rec_name, "darshan-apcxi-rtr-%d-%d-%d-%d",
            apcxi_runtime->group, apcxi_runtime->chassis, apcxi_runtime->slot, apcxi_runtime->blade);
    //apcxi_runtime->rtr_id = darshan_core_gen_record_id(rtr_rec_name);
    apcxi_runtime->rtr_id = darshan_core_gen_record_id("APCXI");
    apcxi_runtime->perf_record = darshan_core_register_record(
            apcxi_runtime->rtr_id,
            //NULL,
            "APCXI",   // we want the record for each rank to be treated as shared records so that mpi_redux can operate on
            //rtr_rec_name,
            DARSHAN_APCXI_MOD,
            APCXI_PERF_RECORD_SIZE(num_nics),
            //sizeof(struct darshan_apcxi_perf_record),
            NULL);
    if(!(apcxi_runtime->perf_record))
    {
        darshan_core_unregister_module(DARSHAN_APCXI_MOD);
        free(apcxi_runtime);
        apcxi_runtime = NULL;
        APCXI_UNLOCK();
        return;
    }
    apcxi_runtime->perf_record->num_nics = num_nics;
    initialize_counters();
    APCXI_UNLOCK();

    return;
}

/********************************************************************************
 * shutdown function exported by this module for coordinating with darshan-core *
 ********************************************************************************/

/* Pass data for the apcxi module back to darshan-core to log to file. */
//#ifdef HAVE_MPI
static void apcxi_mpi_redux(
        void *apcxi_buf,
        MPI_Comm mod_comm,
        darshan_record_id *shared_recs,
        int shared_rec_count)
{
    int i;
    int color;
    int router_rank;
    int router_count;
    int chassis_count;
    int slot_count;
    int group_count;
    unsigned int *bitvec;
    unsigned int bitlen;
    unsigned int bitcnt;
    unsigned int bitsiz;
    MPI_Comm router_comm;

    APCXI_LOCK();
    if (!apcxi_runtime)
    {
        APCXI_UNLOCK();
        return;
    }

    bitcnt = sizeof(unsigned int) * 8;
    bitlen = sizeof(unsigned int) * (MAX_BLADES/bitcnt);
    bitsiz = bitlen / sizeof(unsigned int);
    bitvec = malloc(bitlen);

    /* collect perf counters */
    capture(apcxi_runtime->perf_record, apcxi_runtime->rtr_id);

    if (my_rank == 0)
    {
        apcxi_runtime->header_record->appid = atoi((char*)getenv( APP_ID ));
    }
#if 1
    /* count network dimensions */
    if (bitvec)
    {
        int idx;
        unsigned int uchassis;
        unsigned int uslot;
        unsigned int ublade;

        /* group */
        memset(bitvec, 0, bitlen);
        idx = apcxi_runtime->group / bitcnt;
        bitvec[idx] |= (1 << apcxi_runtime->group % bitcnt);
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
        uchassis = apcxi_runtime->group * 8 + apcxi_runtime->chassis;
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

        /* slot */
        memset(bitvec, 0, bitlen);
        uslot = uchassis * 8 + apcxi_runtime->slot;
        idx = uslot / bitcnt;
        bitvec[idx] |= (1 << uslot % bitcnt);
        PMPI_Reduce((my_rank ? bitvec : MPI_IN_PLACE),
                bitvec,
                bitsiz,
                MPI_INT,
                MPI_BOR,
                0,
                MPI_COMM_WORLD);
        slot_count = count_bits(bitvec, bitsiz);

        /* blade */
        memset(bitvec, 0, bitlen);
        ublade = uchassis * 8 + uslot * 8 + apcxi_runtime->blade;
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
            apcxi_runtime->header_record->nblades = router_count;
            apcxi_runtime->header_record->nchassis = chassis_count;
            apcxi_runtime->header_record->nslots = slot_count;
            apcxi_runtime->header_record->ngroups  = group_count;
        }
        free(bitvec);
    }
    else
    {
        apcxi_runtime->header_record->nblades = 0;
        apcxi_runtime->header_record->nslots = 0;
        apcxi_runtime->header_record->nchassis = 0;
        apcxi_runtime->header_record->ngroups  = 0;
    }

    /*
     * reduce data
     *
     *  aggregate data from processes which share the same node and avg.
     *  create a communicator with all the ranks that share the same node
     *  
     */ 
    color = (apcxi_runtime->group << (7+3)) + \
            (apcxi_runtime->chassis << (3+4)) + \
            (apcxi_runtime->slot << (3+1)) + \
            (apcxi_runtime->blade << 1) + apcxi_runtime->node;
#endif
#if 1
    PMPI_Comm_split(MPI_COMM_WORLD, color, my_rank, &router_comm);
    PMPI_Comm_rank(router_comm,  &router_rank);
    PMPI_Comm_size(router_comm,  &router_count);
    /*
       PMPI_Reduce((router_rank?apcxi_runtime->perf_record->counters:MPI_IN_PLACE),
       apcxi_runtime->perf_record->counters,
       APCXI_NUM_INDICES,
       MPI_LONG_LONG_INT,
       MPI_SUM,
       0,
       router_comm);
     */
    if (router_rank == 0)
    {
        /*
           for (i = 0; i < APCXI_NUM_INDICES; i++)
           {
           apcxi_runtime->perf_record->counters[i] /= router_count;
           }
         */
        apcxi_runtime->perf_record_marked = -1;
    }
    PMPI_Comm_free(&router_comm);
#endif
    APCXI_UNLOCK();
    return;
}

//#endif
static void apcxi_output(
        void **apcxi_buf,
        int *apcxi_buf_sz)
{
    APCXI_LOCK();
    assert(apcxi_runtime);
    *apcxi_buf_sz = 0; 

    if (my_rank == 0) { 
        *apcxi_buf_sz += sizeof(*apcxi_runtime->header_record); 
    }

    if (apcxi_runtime->perf_record_marked == -1) 
    { 
        //*apcxi_buf_sz += sizeof( *apcxi_runtime->perf_record);
        *apcxi_buf_sz += APCXI_PERF_RECORD_SIZE(apcxi_runtime->num_nics);
    }

    APCXI_UNLOCK();
    return;
}

static void apcxi_cleanup()
{
    APCXI_LOCK();
    assert(apcxi_runtime);
    finalize_counters();
    free(apcxi_runtime);
    apcxi_runtime = NULL;
    APCXI_UNLOCK();
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
