#include <stdio.h>
#include <stdint.h>

int main(int argc, char** argv)
{
        int64_t f;
        int64_t n;
        int64_t newf;
        int64_t diff;

        for(f = 1; f < 12000000L; f++)
        {
          n = f * (1L << 28)/ 25000000L;
          newf = (25000000L * n + 0x0fffffff)/ (1L << 28) ;
          diff = newf - f;
          //   if(diff != 0)
          {
            printf("f: %ld newf: %ld diff: %ld \n",
                   (int32_t)f, (int32_t)newf, (int32_t)diff);
          }
        }
        return 0;
}
