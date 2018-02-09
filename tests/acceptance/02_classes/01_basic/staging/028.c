#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#define SPLAY_PSEUDO_RANDOM_CONSTANT 8192

#define DAILY 12*24
main() {
    char s[] = "xx";
    char c,c2;
    int hash, box, minblocks;
    int period=DAILY-1;
    char *boxes[DAILY];
    for (box=0; box<DAILY; box++) {
	boxes[box] = 0;
    }
    for (c='A';c <='z'; c++){
      for (c2='A';c2 <='z'; c2++){
	if (!(isalnum(c) && isalnum(c2))) continue;
	s[0] = c;
	s[1] = c2;
	// The block algorithm is copied from evalfunction.c
	hash=OatHash(s);
	box = (int)(0.5 + period*hash/(double)SPLAY_PSEUDO_RANDOM_CONSTANT);
	// Back to original code
	if (!boxes[box]) {
	    boxes[box] = strncpy((char *)malloc(3),s,3);
	}
      }
    }
    printf("    \"ok\" xor => {\n");
    for (box=0; box<DAILY; box++) {
	printf("\tsplayclass(\"%s\",\"daily\"), # Box %d\n", boxes[box], box);
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

