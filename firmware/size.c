#include <stdio.h>
#include <stdint.h>

uint16_t ary[] = { 0, 1, 2, 3, 4, 5};

uint32_t ary2[] = { 6, 7, 8, 9};

int twodim[5][9];

static const int a = sizeof(ary)/sizeof(ary[0]);
static const int a2 = sizeof(ary2)/sizeof(ary2[0]);
static const int td = sizeof(twodim) / sizeof(twodim[0][0]);


int main(int argc, char** argv)
{
        int entries = sizeof(ary) / sizeof(ary[0]);
        printf("Number of entries:%d a:%d a2:%d\n\n", entries, a, a2);
        printf("Number in twodim: %d \n\n", td);
        return 0;
}

        
