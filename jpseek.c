/*
 * jpseek.c - seek a file in a jpeg
 *
 * Copyright 1998 Allan Latham <alatham@flexsys-group.com>
 *
 * Use permitted under terms of GNU Public Licence only.
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>		
#include <sys/time.h>		
#include <sys/resource.h>		
#include <sys/stat.h>		
#include "bf.h"
#include "cdjpeg.h"
#include "version.h"

#define NKSTREAMS 4

static jvirt_barray_ptr * coef_arrays;
static unsigned int  cpos[NKSTREAMS];
static Blowfish_Data cdata[NKSTREAMS];
static Blowfish_Data lendata;
static Blowfish_Key  bkey;
static int length;
static int tail;
static int tail_on;
static int coef;
static int mode;
static int spos;
static int lh;
static int lt;
static int lw;
static int hib[3];
static int wib[3];
static int d;
static int f;

#include "ltable.h"

static int get_code_bit(int k)
{
 unsigned int a,b,c;
 unsigned char x,z;

 b = cpos[k] & 63;
 if (b == 0) {
  Blowfish_Encrypt(cdata[k],cdata[k],bkey);
 }
 a = b >> 3;
 x = ((unsigned char*)(cdata[k]))[a];
 c = b & 7;
 z = x << c;
 z = z >> 7;
 cpos[k]++;
 return(z);
}

static int demerge_word(int word) {
 int z;

 if (word < 0) {
  z = 0 - word;
 } else {
  z = word;
 }
 if (mode < 0) {
  z &= 2;
  z = z >> 1;
 } else {
  z &= 1;
 }
 return(z);
}

static int get_word(int *value)
{
 int y, ok;

 for(;;) {
  lw += 64;
  if (lw > wib[coef]) {
   lw = spos;
   lh++;
   if (! (lh < hib[coef])) {
    lt += 3;
    if (ltab[lt] < 0) {
     return(1);
    }
    if (tail < 0) {
     if (tail_on == 2) {
      tail_on = 3;
      tail = 999999;
     }
     if (tail_on == 1) {
      tail_on = 2;
      tail = TAIL3;
     }
     if (tail_on == 0) {
      tail_on = 1;
      tail = TAIL2;
     }
    }
    coef = ltab [lt];
    lw = spos = ltab [lt+1];
    lh = 0;
    mode = ltab [lt+2];
   }
  } 
  y = (((short**)(((void**)(((void**)coef_arrays)[coef]))[0]))[lh])[lw]; 
  if ((coef) || (lh) || (lw > 7)) {
   ok = 1;
  } else {
   ok = 0;
  }
  if (ok) {
   if (mode < 0) {
    if ((y > -mode) || (y < mode)) {
     ok = get_code_bit(0);
     ok = ok << 1;
     ok |= get_code_bit(0);
    } else {
     ok = 0;
    }
   } else {
    if (mode == 3) {
     ok = get_code_bit(0);
    } else { 
     ok = 1;
    }
    if (ok) {
     if ((y > 1) || (y < -1)) {
      ok = 0;
     } else { 
      ok = get_code_bit(0);
      if (mode) {
       ok = ok << 1;
       ok |= get_code_bit(0);
      }
     }
     if (ok) {
      ok = 0;
     } else {
      if (mode > 1) {
       ok = get_code_bit(0);
      } else {
       ok = 1;
      }
     }
    }
   }
  }
  if ((ok) && (tail_on > 0)) {
   ok = get_code_bit(2);
  }
  if ((ok) && (tail_on > 1)) {
   ok = get_code_bit(2);
  }
  if ((ok) && (tail_on > 2)) {
   ok = get_code_bit(2);
  }
  if (ok) {
   *value = y;
   return(0);
  }
 }
}

static int get_bit()
{
 int y;

 if (get_word(&y)) {
  return(-1);
 }
 return (demerge_word(y));
}


static int jpseek (const char* infilename,
                   const char* seekfilename)
{
 int b,count,i,j,len0,len1,len2;
 char *pass;
 unsigned char iv[9];
 unsigned char v;
 struct jpeg_decompress_struct srcinfo;
 struct jpeg_error_mgr jsrcerr;
 FILE * input_file;



 input_file = fopen(infilename,"r");
 if (input_file == NULL) {
  perror("Can't open input file");
  return (1);
 }	

 if ((f = open(seekfilename,O_WRONLY|O_TRUNC|O_CREAT)) < 0) {
  perror("Can't open seek file");
  return (1);
 }

 srcinfo.err = jpeg_std_error(&jsrcerr);
 jpeg_create_decompress(&srcinfo);
 jpeg_stdio_src(&srcinfo, input_file);
 jpeg_read_header(&srcinfo, TRUE);

 coef_arrays = jpeg_read_coefficients(&srcinfo);
 
 i = srcinfo.num_components;
 if (i != 3) {
  fprintf(stderr,"Unsuitable input jpeg file: No of components = %d\n", i);
  exit(1);
 }

 for(i=0;i<3;i++) {
  hib[i] = srcinfo.comp_info[i].height_in_blocks;
  wib[i] = 64 * srcinfo.comp_info[i].width_in_blocks - 1;
 }

 for(i=0;i<8;i++) {
  iv[i] = (((short**)(((void**)(((void**)coef_arrays)[0]))[0]))[0])[i]; 
 }

 pass = getpass("Passphrase: ");
 if (strlen(pass) == 0) {
  fprintf(stderr,"Nothing done\n");
  exit(0);
 }  
 if (strlen(pass) > 120) {
  fprintf(stderr,"Truncated to 120 characters\n");
  pass[120] = 0;
 }

 Blowfish_ExpandUserKey(pass,strlen(pass),bkey);

 for(i=0;i<NKSTREAMS;i++) {
  cpos[i] = 0;
  memcpy(cdata+i,iv,8);
  Blowfish_Encrypt(cdata[i],cdata[i],bkey);
  iv[8] = iv[0]; 
  for(j=0;j<8;j++) {
   iv[j] = iv[j+1]; 
  }
 }

 coef = ltab [0];
 spos = ltab [1];
 mode = ltab [2];
 d = 0;
 lh = 0;
 lw = spos -64;
 lt = 0;

 for(i=0;i<8;i++) {
  v = 0;
  for(j=0;j<8;j++) {
   if ((b = get_bit()) < 0) {
    fprintf(stderr,"File not completely recovered\n");
    exit(1);
   }
   b = b << j;
   v |= b;
  }
  ((unsigned char*)lendata)[i] = v;
 }

 Blowfish_Decrypt(lendata,lendata,bkey);

 len0 = ((unsigned char*)lendata)[0];
 len1 = ((unsigned char*)lendata)[1];
 len2 = ((unsigned char*)lendata)[2];

 length = (len0 << 16) | (len1 << 8) | len2;
 tail = length * 8 - TAIL1;
 tail_on = 0;

// fprintf(stderr,"length %d\n",length);

 count = 0;
 while(count < length) {
  for(j=0;j<8;j++) {
   if ((b = get_bit()) < 0) {
    fprintf(stderr,"File not completely recovered\n");
    exit(1);
   }
   b ^= get_code_bit(1);
   v = v << 1;
   v |= b;
   tail--;
  }
  write(f,&v,1);
  count++;
 }

 jpeg_finish_decompress(&srcinfo);
 jpeg_destroy_decompress(&srcinfo);

 close(f);
 fclose(input_file);

 return(0);

}

static void intro()
{
 struct rlimit	rlim;

 printf("\njpseek, version %d.%d (c) 1998 Allan Latham <alatham@flexsys-group.com>\n\n", HS_MAJOR_VERSION, HS_MINOR_VERSION);
 printf("This is licenced software but no charge is made for its use.\n\
NO WARRANTY whatsoever is offered with this product.\n\
NO LIABILITY whatsoever is accepted for its use.\n\
You are using this entirely at your OWN RISK.\n\
See the GNU Public Licence for full details.\n\n");

 rlim.rlim_cur = 0;
 rlim.rlim_max = 0;
 setrlimit(RLIMIT_CORE, &rlim);
}

static void usage()
{
 fprintf(stderr,"Usage:\n\n\
jpseek input-file seek-file\n\n");
 exit(1);
}

int main(int argc, char **argv)
{
 intro();
 if (argc != 3) usage();
 return (jpseek(argv[1],argv[2]));
}

