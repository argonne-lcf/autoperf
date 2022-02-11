/*
 * Copyright (C) 2018 University of Chicago.
 * See COPYRIGHT notice in top-level directory.
 *
 */

#define _GNU_SOURCE
#include "darshan-util-config.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include "darshan-logutils.h"
#include "darshan-apcxi-log-format.h"

/* counter name strings for the apcxi module */
#define Y(a) #a,
#define X(a) Y(APCXI_ ## a)
#define Z(a) #a
char *apcxi_counter_names[] = {
    APCXI_PERF_COUNTERS
};
#undef Y
#undef X
#undef Z

static int darshan_log_get_apcxi_rec(darshan_fd fd, void** buf_p);
static int darshan_log_put_apcxi_rec(darshan_fd fd, void* buf);
static void darshan_log_print_apcxi_rec(void *file_rec,
    char *file_name, char *mnt_pt, char *fs_type);
static void darshan_log_print_apcxi_description(int ver);
static void darshan_log_print_apcxi_rec_diff(void *file_rec1, char *file_name1,
    void *file_rec2, char *file_name2);

struct darshan_mod_logutil_funcs apcxi_logutils =
{
    .log_get_record = &darshan_log_get_apcxi_rec,
    .log_put_record = &darshan_log_put_apcxi_rec,
    .log_print_record = &darshan_log_print_apcxi_rec,
    .log_print_description = &darshan_log_print_apcxi_description,
    .log_print_diff = &darshan_log_print_apcxi_rec_diff,
    .log_agg_records = NULL
};

static int darshan_log_get_apcxi_rec(darshan_fd fd, void** buf_p)
{
    struct darshan_apcxi_header_record *hdr_rec;
    struct darshan_apcxi_perf_record *prf_rec;
    int rec_len;
    char *buffer;
    int i;
    int ret = -1;
    static int first_rec = 1;

    if(fd->mod_map[DARSHAN_APCXI_MOD].len == 0)
        return(0);

    if(fd->mod_ver[DARSHAN_APCXI_MOD] == 0 ||
        fd->mod_ver[DARSHAN_APCXI_MOD] > APCXI_VER)
    {
        fprintf(stderr, "Error: Invalid APCXI module version number (got %d)\n",
            fd->mod_ver[DARSHAN_APCXI_MOD]);
        return(-1);
    }

    if (!*buf_p)
    {
        /* assume this is the largest possible record size */
        buffer = malloc(sizeof(struct darshan_apcxi_perf_record));
        if (!buffer)
        {
            return(-1);
        }
    }
    else
    {
        buffer = *buf_p;
    }

    if (fd->mod_ver[DARSHAN_APCXI_MOD] < APCXI_VER)
    {
        /* perform conversion as needed */
    }

    /* v1, current version */
    if (fd->mod_ver[DARSHAN_APCXI_MOD] == APCXI_VER)
    {
        if (first_rec)
        {
            rec_len = sizeof(struct darshan_apcxi_header_record);
            first_rec = 0;
        }
        else
            rec_len = sizeof(struct darshan_apcxi_perf_record);

        ret = darshan_log_get_mod(fd, DARSHAN_APCXI_MOD, buffer, rec_len);
    }

    if (ret == rec_len)
    {
        if(fd->swap_flag)
        {
            if (rec_len == sizeof(struct darshan_apcxi_header_record))
            {
                hdr_rec = (struct darshan_apcxi_header_record*)buffer; 
                /* swap bytes if necessary */
                DARSHAN_BSWAP64(&(hdr_rec->base_rec.id));
                DARSHAN_BSWAP64(&(hdr_rec->base_rec.rank));
                DARSHAN_BSWAP64(&(hdr_rec->magic));
                DARSHAN_BSWAP64(&(hdr_rec->nblades));
                DARSHAN_BSWAP64(&(hdr_rec->nslots));
                DARSHAN_BSWAP64(&(hdr_rec->nchassis));
                DARSHAN_BSWAP64(&(hdr_rec->ngroups));
                DARSHAN_BSWAP64(&(hdr_rec->appid));
            }
            else
            {
                prf_rec = (struct darshan_apcxi_perf_record*)buffer;
                DARSHAN_BSWAP64(&(prf_rec->base_rec.id));
                DARSHAN_BSWAP64(&(prf_rec->base_rec.rank));
                DARSHAN_BSWAP64(&(prf_rec->group));
                DARSHAN_BSWAP64(&(prf_rec->chassis));
                DARSHAN_BSWAP64(&(prf_rec->slot));
                DARSHAN_BSWAP64(&(prf_rec->blade));
                DARSHAN_BSWAP64(&(prf_rec->node));
                for (i = 0; i < APCXI_NUM_INDICES; i++)
                {
                    DARSHAN_BSWAP64(&prf_rec->counters[i]);
                }
            }
        }
        *buf_p = buffer;
        return(1);
    }
    else if (ret < 0)
    {
     //*buf_p = NULL;
     //  if (buffer) free(buffer);
        if (!*buf_p) free(buffer);
        return(-1);
    }
    else
    {
     //  *buf_p = NULL;
     //  if (buffer) free(buffer);
        if (!*buf_p) free(buffer);
        return(0);
    }
}

static int darshan_log_put_apcxi_rec(darshan_fd fd, void* buf)
{
    int ret;
    int rec_len;
    static int first_rec = 1;

    if (first_rec)
    {
        rec_len = sizeof(struct darshan_apcxi_header_record);
        first_rec = 0;
    }
    else
        rec_len = sizeof(struct darshan_apcxi_perf_record);
    
    ret = darshan_log_put_mod(fd, DARSHAN_APCXI_MOD, buf,
                              rec_len, APCXI_VER);
    if(ret < 0)
        return(-1);

    return(0);
}

static void darshan_log_print_apcxi_rec(void *rec, char *file_name,
    char *mnt_pt, char *fs_type)
{
    int i;
    static int first_rec = 1;
    struct darshan_apcxi_header_record *hdr_rec;
    struct darshan_apcxi_perf_record *prf_rec;
    
    if (first_rec)
    { 
        hdr_rec = rec;
        DARSHAN_D_COUNTER_PRINT(darshan_module_names[DARSHAN_APCXI_MOD],
            hdr_rec->base_rec.rank, hdr_rec->base_rec.id,
            "APCXI_GROUPS", hdr_rec->ngroups, "", "", "");
        DARSHAN_D_COUNTER_PRINT(darshan_module_names[DARSHAN_APCXI_MOD],
            hdr_rec->base_rec.rank, hdr_rec->base_rec.id,
            "APCXI_CHASSIS", hdr_rec->nchassis, "", "", "");
        DARSHAN_D_COUNTER_PRINT(darshan_module_names[DARSHAN_APCXI_MOD],
            hdr_rec->base_rec.rank, hdr_rec->base_rec.id,
            "APCXI_SLOTS", hdr_rec->nslots, "", "", "");
        DARSHAN_D_COUNTER_PRINT(darshan_module_names[DARSHAN_APCXI_MOD],
            hdr_rec->base_rec.rank, hdr_rec->base_rec.id,
            "APCXI_BLADES", hdr_rec->nblades, "", "", "");
        DARSHAN_U_COUNTER_PRINT(darshan_module_names[DARSHAN_APCXI_MOD],
            hdr_rec->base_rec.rank, hdr_rec->base_rec.id,
            "APCXI_APPLICATION_ID", hdr_rec->appid, "", "", "");
        first_rec = 0;
    }
    else
    {
        prf_rec = rec;
        
        DARSHAN_D_COUNTER_PRINT(darshan_module_names[DARSHAN_APCXI_MOD],
            prf_rec->base_rec.rank, prf_rec->base_rec.id,
            "APCXI_GROUP", prf_rec->group, "", "", "");
        DARSHAN_D_COUNTER_PRINT(darshan_module_names[DARSHAN_APCXI_MOD],
            prf_rec->base_rec.rank, prf_rec->base_rec.id,
            "APCXI_CHASSIS", prf_rec->chassis, "", "", "");
        DARSHAN_D_COUNTER_PRINT(darshan_module_names[DARSHAN_APCXI_MOD],
            prf_rec->base_rec.rank, prf_rec->base_rec.id,
            "APCXI_SLOT", prf_rec->slot, "", "", "");
        DARSHAN_D_COUNTER_PRINT(darshan_module_names[DARSHAN_APCXI_MOD],
            prf_rec->base_rec.rank, prf_rec->base_rec.id,
            "APCXI_BLADE", prf_rec->blade, "", "", "");
        DARSHAN_D_COUNTER_PRINT(darshan_module_names[DARSHAN_APCXI_MOD],
            prf_rec->base_rec.rank, prf_rec->base_rec.id,
            "APCXI_NODE", prf_rec->node, "", "", "");

        for(i = 0; i < APCXI_NUM_INDICES; i++)
        {
            
            DARSHAN_U_COUNTER_PRINT(darshan_module_names[DARSHAN_APCXI_MOD],
                prf_rec->base_rec.rank, prf_rec->base_rec.id,
                apcxi_counter_names[i], prf_rec->counters[i],
                "", "", "");
        }
    }

    return;
}

static void darshan_log_print_apcxi_description(int ver)
{
    printf("\n# description of apcxi counters:\n");
    printf("#   global summary stats for the apcxi module:\n");
    printf("#     APCXI_GROUPS: total number of groups\n");
    printf("#     APCXI_CHASSIS: total number of chassis\n");
    printf("#     APCXI_BLADES: total number of blades\n");
    printf("#     APCXI_SLOTS: total number of slots\n");
    printf("#   per-router statistics for the apcxi module:\n");
    printf("#     APCXI_GROUP:   group this router is on\n");
    printf("#     APCXI_CHASSIS: chassis this router is on\n");
    printf("#     APCXI_SLOT:   slot in the chassis this router is on\n");
    printf("#     APCXI_BLADE:   blade this router is on\n");
    printf("#     APCXI_NODE:    node connected to this router\n");
    printf("#     APCXI_AR_RTR_* port counters for the 40 router-router ports\n");
    //printf("#     APCXI_AR_RTR_x_y_INQ_PRF_INCOMING_FLIT_VC[0-7]: flits on VCs of x y tile\n");
    //printf("#     APCXI_AR_RTR_x_y_INQ_PRF_ROWBUS_STALL_CNT: stalls on x y tile\n");
    //printf("#     APCXI_AR_RTR_PT_* port counters for the 8 router-nic ports\n");
    //printf("#     APCXI_AR_RTR_PT_x_y_INQ_PRF_INCOMING_FLIT_VC[0,4]: flits on VCs of x y tile\n");
    //printf("#     APCXI_AR_RTR_PT_x_y_INQ_PRF_REQ_ROWBUS_STALL_CNT: stalls on x y tile\n"); 

    return;
}

static void darshan_log_print_apcxi_rec_diff(void *file_rec1, char *file_name1,
    void *file_rec2, char *file_name2)
{
    struct darshan_apcxi_header_record *hdr_rec1;
    struct darshan_apcxi_header_record *hdr_rec2;
    struct darshan_apcxi_perf_record   *prf_rec1;
    struct darshan_apcxi_perf_record   *prf_rec2;

    hdr_rec1 = (struct darshan_apcxi_header_record*) file_rec1;
    hdr_rec2 = (struct darshan_apcxi_header_record*) file_rec2;
    prf_rec1 = (struct darshan_apcxi_perf_record*) file_rec1;
    prf_rec2 = (struct darshan_apcxi_perf_record*) file_rec2;

    if (hdr_rec1->magic == APCXI_MAGIC)
    {
        /* this is the header record */
        if (!hdr_rec2)
        {
            printf("- ");
            DARSHAN_D_COUNTER_PRINT(darshan_module_names[DARSHAN_APCXI_MOD],
                hdr_rec1->base_rec.rank, hdr_rec1->base_rec.id,
                "APCXI_GROUPS", hdr_rec1->ngroups, "", "", "");
            printf("- ");
            DARSHAN_D_COUNTER_PRINT(darshan_module_names[DARSHAN_APCXI_MOD],
                hdr_rec1->base_rec.rank, hdr_rec1->base_rec.id,
                "APCXI_CHASSIS", hdr_rec1->nchassis, "", "", "");
            printf("- ");
            DARSHAN_D_COUNTER_PRINT(darshan_module_names[DARSHAN_APCXI_MOD],
                hdr_rec1->base_rec.rank, hdr_rec1->base_rec.id,
                "APCXI_SLOTS", hdr_rec1->nslots, "", "", "");
            printf("- ");
            DARSHAN_D_COUNTER_PRINT(darshan_module_names[DARSHAN_APCXI_MOD],
                hdr_rec1->base_rec.rank, hdr_rec1->base_rec.id,
                "APCXI_BLADES", hdr_rec1->nblades, "", "", "");
            printf("- ");
            DARSHAN_U_COUNTER_PRINT(darshan_module_names[DARSHAN_APCXI_MOD],
                hdr_rec1->base_rec.rank, hdr_rec1->base_rec.id,
                "APCXI_APPLICATION_ID", hdr_rec1->appid, "", "", "");
        }
        else if (!hdr_rec1)
        {
            printf("+ ");
            DARSHAN_D_COUNTER_PRINT(darshan_module_names[DARSHAN_APCXI_MOD],
                hdr_rec2->base_rec.rank, hdr_rec2->base_rec.id,
                "APCXI_GROUPS", hdr_rec2->ngroups, "", "", "");
            printf("+ ");
            DARSHAN_D_COUNTER_PRINT(darshan_module_names[DARSHAN_APCXI_MOD],
                hdr_rec2->base_rec.rank, hdr_rec2->base_rec.id,
                "APCXI_CHASSIS", hdr_rec2->nchassis, "", "", "");
            printf("+ ");
            DARSHAN_D_COUNTER_PRINT(darshan_module_names[DARSHAN_APCXI_MOD],
                hdr_rec2->base_rec.rank, hdr_rec2->base_rec.id,
                "APCXI_SLOTS", hdr_rec2->nslots, "", "", "");
            printf("+ ");
            DARSHAN_D_COUNTER_PRINT(darshan_module_names[DARSHAN_APCXI_MOD],
                hdr_rec2->base_rec.rank, hdr_rec2->base_rec.id,
                "APCXI_BLADES", hdr_rec2->nblades, "", "", "");
            printf("+ ");
            DARSHAN_U_COUNTER_PRINT(darshan_module_names[DARSHAN_APCXI_MOD],
                hdr_rec2->base_rec.rank, hdr_rec2->base_rec.id,
                "APCXI_APPLICATION_ID", hdr_rec2->appid, "", "", "");
        }
        else
        {
            if (hdr_rec1->ngroups != hdr_rec2->ngroups)
            {
                printf("- ");
                DARSHAN_D_COUNTER_PRINT(darshan_module_names[DARSHAN_APCXI_MOD],
                hdr_rec1->base_rec.rank, hdr_rec1->base_rec.id,
                "APCXI_GROUPS", hdr_rec1->ngroups, "", "", "");
                printf("+ ");
                DARSHAN_D_COUNTER_PRINT(darshan_module_names[DARSHAN_APCXI_MOD],
                hdr_rec2->base_rec.rank, hdr_rec2->base_rec.id,
                "APCXI_GROUPS", hdr_rec2->ngroups, "", "", "");
            }
            if (hdr_rec1->nchassis != hdr_rec2->nchassis)
            {
                printf("- ");
                DARSHAN_D_COUNTER_PRINT(darshan_module_names[DARSHAN_APCXI_MOD],
                hdr_rec1->base_rec.rank, hdr_rec1->base_rec.id,
                "APCXI_CHASSIS", hdr_rec1->nchassis, "", "", "");
                printf("+ ");
                DARSHAN_D_COUNTER_PRINT(darshan_module_names[DARSHAN_APCXI_MOD],
                hdr_rec2->base_rec.rank, hdr_rec2->base_rec.id,
                "APCXI_CHASSIS", hdr_rec2->nchassis, "", "", "");
            }
            if (hdr_rec1->nslots != hdr_rec2->nslots)
            {
                printf("- ");
                DARSHAN_D_COUNTER_PRINT(darshan_module_names[DARSHAN_APCXI_MOD],
                hdr_rec1->base_rec.rank, hdr_rec1->base_rec.id,
                "APCXI_SLOTS", hdr_rec1->nslots, "", "", "");
                printf("+ ");
                DARSHAN_D_COUNTER_PRINT(darshan_module_names[DARSHAN_APCXI_MOD],
                hdr_rec2->base_rec.rank, hdr_rec2->base_rec.id,
                "APCXI_SLOTS", hdr_rec2->nslots, "", "", "");
            }
            if (hdr_rec1->nblades != hdr_rec2->nblades)
            {
                printf("- ");
                DARSHAN_D_COUNTER_PRINT(darshan_module_names[DARSHAN_APCXI_MOD],
                hdr_rec1->base_rec.rank, hdr_rec1->base_rec.id,
                "APCXI_BLADES", hdr_rec1->nblades, "", "", "");

                printf("+ ");
                DARSHAN_D_COUNTER_PRINT(darshan_module_names[DARSHAN_APCXI_MOD],
                hdr_rec2->base_rec.rank, hdr_rec2->base_rec.id,
                "APCXI_BLADES", hdr_rec2->nblades, "", "", "");
            }
            if (hdr_rec1->appid != hdr_rec2->appid)
            {
                printf("- ");
                DARSHAN_U_COUNTER_PRINT(darshan_module_names[DARSHAN_APCXI_MOD],
                hdr_rec1->base_rec.rank, hdr_rec1->base_rec.id,
                "APCXI_APPLICATION_ID", hdr_rec1->appid, "", "", "");

                printf("+ ");
                DARSHAN_U_COUNTER_PRINT(darshan_module_names[DARSHAN_APCXI_MOD],
                hdr_rec2->base_rec.rank, hdr_rec2->base_rec.id,
                "APCXI_APPLICATION_ID", hdr_rec2->appid, "", "", "");
            }
        }
    }
    else
    {
        if (!prf_rec2)
        {
            printf("- ");
            DARSHAN_D_COUNTER_PRINT(darshan_module_names[DARSHAN_APCXI_MOD],
                    prf_rec1->base_rec.rank, prf_rec1->base_rec.id,
                    "APCXI_GROUP", prf_rec1->group, "", "", "");
            printf("- ");
            DARSHAN_D_COUNTER_PRINT(darshan_module_names[DARSHAN_APCXI_MOD],
                    prf_rec1->base_rec.rank, prf_rec1->base_rec.id,
                    "APCXI_CHASSIS", prf_rec1->chassis, "", "", "");
            printf("- ");
            DARSHAN_D_COUNTER_PRINT(darshan_module_names[DARSHAN_APCXI_MOD],
                    prf_rec1->base_rec.rank, prf_rec1->base_rec.id,
                    "APCXI_SLOT", prf_rec1->slot, "", "", "");
            printf("- ");
            DARSHAN_D_COUNTER_PRINT(darshan_module_names[DARSHAN_APCXI_MOD],
                    prf_rec1->base_rec.rank, prf_rec1->base_rec.id,
                    "APCXI_BLADE", prf_rec1->blade, "", "", "");
            printf("- ");
            DARSHAN_D_COUNTER_PRINT(darshan_module_names[DARSHAN_APCXI_MOD],
                    prf_rec1->base_rec.rank, prf_rec1->base_rec.id,
                    "APCXI_NODE", prf_rec1->node, "", "", "");
        }
        else if (!prf_rec1)
        {
            printf("+ ");
            DARSHAN_D_COUNTER_PRINT(darshan_module_names[DARSHAN_APCXI_MOD],
                    prf_rec2->base_rec.rank, prf_rec2->base_rec.id,
                    "APCXI_GROUP", prf_rec2->group, "", "", "");
            printf("+ ");
            DARSHAN_D_COUNTER_PRINT(darshan_module_names[DARSHAN_APCXI_MOD],
                    prf_rec2->base_rec.rank, prf_rec2->base_rec.id,
                    "APCXI_CHASSIS", prf_rec2->chassis, "", "", "");
            printf("+ ");
            DARSHAN_D_COUNTER_PRINT(darshan_module_names[DARSHAN_APCXI_MOD],
                    prf_rec2->base_rec.rank, prf_rec2->base_rec.id,
                    "APCXI_SLOT", prf_rec2->slot, "", "", "");
            printf("+ ");
            DARSHAN_D_COUNTER_PRINT(darshan_module_names[DARSHAN_APCXI_MOD],
                    prf_rec2->base_rec.rank, prf_rec2->base_rec.id,
                    "APCXI_BLADE", prf_rec2->blade, "", "", "");
            printf("+ ");
            DARSHAN_D_COUNTER_PRINT(darshan_module_names[DARSHAN_APCXI_MOD],
                    prf_rec2->base_rec.rank, prf_rec2->base_rec.id,
                    "APCXI_NODE", prf_rec2->node, "", "", "");
        }
        else {
            if (prf_rec1->group != prf_rec2->group)
            {
                printf("- ");
                DARSHAN_D_COUNTER_PRINT(darshan_module_names[DARSHAN_APCXI_MOD],
                        prf_rec1->base_rec.rank, prf_rec1->base_rec.id,
                        "APCXI_GROUP", prf_rec1->group, "", "", "");
                printf("+ ");
                DARSHAN_D_COUNTER_PRINT(darshan_module_names[DARSHAN_APCXI_MOD],
                        prf_rec2->base_rec.rank, prf_rec2->base_rec.id,
                        "APCXI_GROUP", prf_rec2->group, "", "", "");
            }
            if (prf_rec1->chassis != prf_rec2->chassis)
            {
                printf("- ");
                DARSHAN_D_COUNTER_PRINT(darshan_module_names[DARSHAN_APCXI_MOD],
                        prf_rec1->base_rec.rank, prf_rec1->base_rec.id,
                        "APCXI_CHASSIS", prf_rec1->chassis, "", "", "");
                printf("+ ");
                DARSHAN_D_COUNTER_PRINT(darshan_module_names[DARSHAN_APCXI_MOD],
                        prf_rec2->base_rec.rank, prf_rec2->base_rec.id,
                        "APCXI_CHASSIS", prf_rec2->chassis, "", "", "");
            }
            if (prf_rec1->slot != prf_rec2->slot)
            {
                printf("- ");
                DARSHAN_D_COUNTER_PRINT(darshan_module_names[DARSHAN_APCXI_MOD],
                        prf_rec1->base_rec.rank, prf_rec1->base_rec.id,
                        "APCXI_SLOT", prf_rec1->slot, "", "", "");
                printf("+ ");
                DARSHAN_D_COUNTER_PRINT(darshan_module_names[DARSHAN_APCXI_MOD],
                        prf_rec2->base_rec.rank, prf_rec2->base_rec.id,
                        "APCXI_SLOT", prf_rec2->slot, "", "", "");
            }
            if (prf_rec1->blade != prf_rec2->blade)
            {
                printf("- ");
                DARSHAN_D_COUNTER_PRINT(darshan_module_names[DARSHAN_APCXI_MOD],
                        prf_rec1->base_rec.rank, prf_rec1->base_rec.id,
                        "APCXI_BLADE", prf_rec1->blade, "", "", "");
                printf("+ ");
                DARSHAN_D_COUNTER_PRINT(darshan_module_names[DARSHAN_APCXI_MOD],
                        prf_rec2->base_rec.rank, prf_rec2->base_rec.id,
                        "APCXI_BLADE", prf_rec2->blade, "", "", "");
            }
            if (prf_rec1->node != prf_rec2->node)
            {
                printf("- ");
                DARSHAN_D_COUNTER_PRINT(darshan_module_names[DARSHAN_APCXI_MOD],
                        prf_rec1->base_rec.rank, prf_rec1->base_rec.id,
                        "APCXI_NODE", prf_rec1->node, "", "", "");
                printf("+ ");
                DARSHAN_D_COUNTER_PRINT(darshan_module_names[DARSHAN_APCXI_MOD],
                        prf_rec2->base_rec.rank, prf_rec2->base_rec.id,
                        "APCXI_NODE", prf_rec2->node, "", "", "");
            }
        } 

        int i;
        /* router tile record */
        for(i = 0; i < APCXI_NUM_INDICES; i++)
        {
            if (!prf_rec2)
            {
                printf("- ");
                DARSHAN_U_COUNTER_PRINT(darshan_module_names[DARSHAN_APCXI_MOD],
                prf_rec1->base_rec.rank, prf_rec1->base_rec.id,
                apcxi_counter_names[i], prf_rec1->counters[i],
                "", "", "");
            }
            else if (!prf_rec1)
            {
                printf("+ ");
                DARSHAN_U_COUNTER_PRINT(darshan_module_names[DARSHAN_APCXI_MOD],
                prf_rec2->base_rec.rank, prf_rec2->base_rec.id,
                apcxi_counter_names[i], prf_rec2->counters[i],
                "", "", "");
            }
            else if (prf_rec1->counters[i] != prf_rec2->counters[i])
            {
                printf("- ");
                DARSHAN_U_COUNTER_PRINT(darshan_module_names[DARSHAN_APCXI_MOD],
                prf_rec1->base_rec.rank, prf_rec1->base_rec.id,
                apcxi_counter_names[i], prf_rec1->counters[i],
                "", "", "");

                printf("+ ");
                DARSHAN_U_COUNTER_PRINT(darshan_module_names[DARSHAN_APCXI_MOD],
                prf_rec2->base_rec.rank, prf_rec2->base_rec.id,
                apcxi_counter_names[i], prf_rec2->counters[i],
                "", "", "");
            }
        }
    }

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
