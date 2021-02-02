#include <stdio.h>
#include "darshan-log-format.h"
#include "darshan-apxc-log-format.h"

int main (int argc, char **argv)
{
   printf ("APXC_NUM_INDICES = %d\n", APXC_NUM_INDICES);
   printf ("sizeof darshan_apxc_header_record = %d\n", sizeof(struct darshan_apxc_header_record));
   printf ("sizeof darshan_apxc_perf_record = %d\n", sizeof(struct darshan_apxc_perf_record));

   return 0;
}
