//
// Created by andrew on 3/25/26.
//

#include <asm-generic/types.h>
#include <stdio.h>
#include "generic.h"

void dump_buffer(void *buffer,__u16 size)
{
    char ret_print[18];
    void *cur_pkt = buffer;
    printf ("%06x\t",0);
    fflush (stdout);
    for(int p = 0; p < size; p++)
    {
        if((p + 1) % 16 == 0)
            snprintf (ret_print,10,"\n%06x\t",p + 1);
        printf ("%02x%s",*(uint8_t *) (cur_pkt + p),
                ((p + 1) % 16 == 0)?ret_print:" ");
        fflush (stdout);
    }
    printf ("\n\n");
    fflush (stdout);
}
