/*
 * jphide.c - hide a file in a jpeg
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

static int okfile;
static int data_pos;
static unsigned char data_char;

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

static void put_word(int coef, int row, int col, int value)
{
 (((short**)(((void**)(((void**)coef_arrays)[coef]))[0]))[row])[col] = value; 
}

static int merge_word(int word, int bit)
{
 int y,z;

 y = word;
 if (mode < 0) {
  z = bit << 1;
  if (y > 0) {
   y &= ~2;
   y |= z;
  } else {
   y = 0 - y;
   y &= ~2;
   y |= z;
   y = 0 - y;
  }
 } else {
  if (y == 0) {
   d = 0 - d;
   y = d * bit;
  } else {
   if ((y == -1) || (y == 1)) {
    d = y;
    y = bit * y;
   } else {
    if (y > 0) {
     d = 1;
     y &= ~1;
     y |= bit;
    } else {
     d = -1;
     y = 0 - y;
     y &= ~1;
     y |= bit;
     y = 0 - y;
    }
   }
  }
 }
 return(y);
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
    if (okfile) {
     lt += 3;
     if (ltab[lt] < 0) {
      return(-1);
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
    } else {
     return(1);
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
  put_word(coef, lh, lw, y);
 }
}

static int get_bit()
{
 unsigned int b,z;

 if (okfile) {
  b = data_pos & 7;
  if (b == 0) {
   if (read(f,&data_char,1) != 1) {
    okfile = 0;
    close(f);
   }
  }
 }
 if (okfile) {
  z = data_char & 128;
  z = z >> 7;
  z = z ^ get_code_bit(1);
  data_char = data_char << 1;
  data_pos++;
  tail--;
 } else {
  z = get_code_bit(2);
 }
 return (z);
}

static int jphide (const char* infilename,
                   const char* outfilename,
                   const char* hidefilename)
{
 int b,i,j,ok,y;
 char *pass;
 char pass1[121];
 char pass2[121];
 unsigned char iv[9];
 unsigned char len0;
 unsigned char len1;
 unsigned char len2;
 struct jpeg_decompress_struct srcinfo;
 struct jpeg_compress_struct dstinfo;
 struct jpeg_error_mgr jsrcerr, jdsterr;
 struct stat ifstats;
 FILE * input_file;
 FILE * output_file;


 if ((f = open(hidefilename,O_RDONLY)) < 0) {
  perror("Can't open hide file");
  return (1);
 }

 fstat(f,&ifstats);

 okfile = 1;
 data_pos = 0;

 input_file = fopen(infilename,"r");
 if (input_file == NULL) {
  perror("Can't open input file");
  return (1);
 }	

 output_file = fopen(outfilename,"w");
 if (output_file == NULL) {
  perror("Can't open output file");
  return (1);
 }	

 srcinfo.err = jpeg_std_error(&jsrcerr);
 dstinfo.err = jpeg_std_error(&jdsterr);
 jpeg_create_decompress(&srcinfo);
 jpeg_create_compress(&dstinfo);
 jpeg_stdio_src(&srcinfo, input_file);
 jpeg_read_header(&srcinfo, TRUE);

 coef_arrays = jpeg_read_coefficients(&srcinfo);
 
 i = srcinfo.num_components;
 if (i != 3) {
  fprintf(stderr,"Unsuitable input jpeg file: No of components = %d\n", i);
  exit(1);
 }

 for(i = 0; i < 3; i++) {
  hib[i] = srcinfo.comp_info[i].height_in_blocks;
  wib[i] = 64 * srcinfo.comp_info[i].width_in_blocks - 1;
 }

 for(i=0;i<8;i++) {
  iv[i] = (((short**)(((void**)(((void**)coef_arrays)[0]))[0]))[0])[i]; 
 }

 do {
  pass = getpass("Passphrase: ");
  if (strlen(pass) == 0) {
   fprintf(stderr,"Nothing done\n");
   exit(0);
  }  
  if (strlen(pass) > 120) {
   fprintf(stderr,"Truncated to 120 characters\n");
   pass[120] = 0;
  }
  strcpy(pass1,pass);
  pass = getpass("Re-enter  : ");
  if (strlen(pass) == 0) {
   fprintf(stderr,"Nothing done\n");
   exit(0);
  }  
  if (strlen(pass) > 120) {
   pass[120] = 0;
  }
  strcpy(pass2,pass);
  if (strcmp(pass1, pass2)) {
   fprintf(stderr,"Passphrases are different - try again\n");
   ok = 1;
  } else {
   ok = 0;
  }
 } while (ok);

 Blowfish_ExpandUserKey(pass1,strlen(pass1),bkey);

 for(i=0;i<NKSTREAMS;i++) {
  cpos[i] = 0;
  memcpy(cdata+i,iv,8);
  Blowfish_Encrypt(cdata[i],cdata[i],bkey);
  iv[8] = iv[0]; 
  for(j=0;j<8;j++) {
   iv[j] = iv[j+1]; 
  }
 }
 memcpy(lendata,iv,8);
 Blowfish_Encrypt(lendata,lendata,bkey);

 coef = ltab [0];
 spos = ltab [1];
 mode = ltab [2];
 d = 0;
 lh = 0;
 lw = spos -64;
 lt = 0;

 length = ifstats.st_size;
 len0 = length >> 16;
 len1 = (length & 0xff00) >> 8;
 len2 = length & 255;
 tail = length * 8 - TAIL1;
 tail_on = 0;

 ((unsigned char*)lendata)[0] = len0;
 ((unsigned char*)lendata)[1] = len1;
 ((unsigned char*)lendata)[2] = len2;

// fprintf(stderr,"length %d\n",length);

 Blowfish_Encrypt(lendata,lendata,bkey);

 for(i=0;i<8;i++) {
  for(j=0;j<8;j++) {
   if (get_word(&y)) {
    fprintf(stderr,"File not completely stored\n");
    exit(1);
   }
   b = ((unsigned char*)lendata)[i];
   b = b >> j;
   b &= 1;
   put_word(coef, lh, lw, merge_word(y, b));
  }
 }

 while ((b = get_word(&y)) == 0) {
  put_word(coef, lh, lw, merge_word(y, get_bit()));
 }
 if (b == -1) {
  fprintf(stderr,"File not completely stored\n");
  exit(1);
 }

 jpeg_copy_critical_parameters(&srcinfo, &dstinfo);
 dstinfo.optimize_coding = TRUE;
 jpeg_stdio_dest(&dstinfo, output_file);
 jpeg_write_coefficients(&dstinfo, coef_arrays);
 jpeg_finish_compress(&dstinfo);
 jpeg_destroy_compress(&dstinfo);
 jpeg_finish_decompress(&srcinfo);
 jpeg_destroy_decompress(&srcinfo);

 close(f);
 fclose(input_file);
 fclose(output_file);

 return(0);

}

static void intro()
{
 struct rlimit	rlim;

 printf("\njphide, version %d.%d (c) 1998 Allan Latham <alatham@flexsys-group.com>\n\n", HS_MAJOR_VERSION, HS_MINOR_VERSION);
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
jphide input-jpg-file output-jpg-file hide-file\n\n");
 exit(1);
}

int main(int argc, char **argv)
{
 intro();
 if (argc != 4) usage();
 return (jphide(argv[1],argv[2],argv[3]));
}

