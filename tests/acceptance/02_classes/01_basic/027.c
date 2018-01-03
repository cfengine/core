#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#define SPLAY_PSEUDO_RANDOM_CONSTANT 8192

#define HOURLY 12
main() {
    char s[] = "a";
    char c;
    int hash, box, minblocks;
    int period=HOURLY-1;
    char *boxes[HOURLY];
    for (c=0; c<HOURLY; c++) {
	boxes[c] = 0;
    }
    for (c='a';c <='z'; c++){
	*s = c;
	// The block algorithm is copied from evalfunction.c
	hash=OatHash(s);
	box = (int)(0.5 + period*hash/(double)SPLAY_PSEUDO_RANDOM_CONSTANT);
	minblocks = box % HOURLY;
	// Back to original code
	if (!boxes[minblocks]) {
	    boxes[minblocks] = strncpy((char *)malloc(2),s,2);
	}
    }
    printf("    \"ok\" xor => {\n");
    for (c=0; c<HOURLY; c++) {
	printf("\tsplayclass(\"%s\",\"hourly\"), # Box %d\n", boxes[c], c);
    }
    printf("\t};\n");
}

// This is copied from files_hashes.c
int OatHash(char *key)
       
{ unsigned int hashtablesize = SPLAY_PSEUDO_RANDOM_CONSTANT;
unsigned char *p = key;
unsigned h = 0;
int i, len = strlen(key);
	 
for ( i = 0; i < len; i++ )
    {       
    h += p[i];
    h += ( h << 10 );
    h ^= ( h >> 6 );
    }       
     
  h += ( h << 3 );
  h ^= ( h >> 11 );
  h += ( h << 15 );
     
  return (h & (hashtablesize-1));
}  

