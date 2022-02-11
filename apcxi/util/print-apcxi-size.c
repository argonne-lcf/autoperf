#include <stdio.h>
#include "darshan-log-format.h"
#include "darshan-apcxi-log-format.h"

int main (int argc, char **argv)
{
   printf ("APCXI_NUM_INDICES = %d\n", APCXI_NUM_INDICES);
   printf ("sizeof darshan_apcxi_header_record = %d\n", sizeof(struct darshan_apcxi_header_record));
   printf ("sizeof darshan_apcxi_perf_record = %d\n", sizeof(struct darshan_apcxi_perf_record));

   return 0;
}
