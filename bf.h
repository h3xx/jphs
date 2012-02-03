
/* The data block processed by the encryption algorithm - 64 bits */
typedef unsigned long Blowfish_Data[2];
/* The key as entered by the user - size may vary */
typedef char Blowfish_UserKey[16];
/* The expanded key for internal use - 18+4*256 words*/
typedef unsigned long Blowfish_Key[1042];

#ifdef Strict_Type_Check
extern void Blowfish_Encrypt(Blowfish_Data dataIn, Blowfish_Data dataOut,
			     Blowfish_Key key);
extern void Blowfish_Decrypt(Blowfish_Data dataIn, Blowfish_Data dataOut,
			     Blowfish_Key key);
extern void Blowfish_ExpandUserKey(Blowfish_UserKey userKey, int userKeyLen,
				   Blowfish_Key key);
#else
extern void Blowfish_Encrypt(void *dataIn, void *dataOut, void *key);
extern void Blowfish_Decrypt(void *dataIn, void *dataOut, void *key);
extern void Blowfish_ExpandUserKey(void *userKey, int userKeyLen, void *key);
#endif
