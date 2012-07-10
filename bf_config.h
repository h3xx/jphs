/*
 * Simple system configuration.
 */

#ifndef _BF_CONFIG_H_
#define _BF_CONFIG_H_

#ifdef BF_LE

  /* force little-endian Blowfish */
# define BF_DONTNEED_BE		1
# define Blowfish_Encrypt	L_Blowfish_Encrypt
# define Blowfish_Decrypt	L_Blowfish_Decrypt

#else

  /* force big-endian Blowfish (default) */
# define BF_DONTNEED_LE		1
# define Blowfish_Encrypt	B_Blowfish_Encrypt
# define Blowfish_Decrypt	B_Blowfish_Decrypt

#endif

#endif
