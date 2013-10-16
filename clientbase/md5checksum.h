#ifndef MD5Checksum_H
#define MD5Checksum_H

#include <stdio.h>
#include <string.h>

/* POINTER defines a generic pointer type */
typedef unsigned char* POINTER;

/* UINT2 defines a two byte word */
typedef unsigned short int UINT2;

/* UINT4 defines a four byte word */
typedef unsigned long int UINT4;

/* Constants for MD5Transform routine. */
#define S11 7
#define S12 12
#define S13 17
#define S14 22
#define S21 5
#define S22 9
#define S23 14
#define S24 20
#define S31 4
#define S32 11
#define S33 16
#define S34 23
#define S41 6
#define S42 10
#define S43 15
#define S44 21

POINTER MDBytes(POINTER data, unsigned int len, POINTER buff);
unsigned char* MD5Data(POINTER data, unsigned int len, unsigned char* buff) ;

typedef struct
{
	UINT4 state[4];									/* state (ABCD) */
	UINT4 count[2];									/* number of bits, modulo 2^64 (lsb first) */
	unsigned char buffer[64];                       /* input buffer */
}MD5_CTX;

static unsigned char PADDING[64] = {
	0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/* F, G, H and I are basic MD5 functions. */
#define F(x, y, z) (((x) & (y)) | ((~x) & (z)))
#define G(x, y, z) (((x) & (z)) | ((y) & (~z)))
#define H(x, y, z) ((x) ^ (y) ^ (z))
#define I(x, y, z) ((y) ^ ((x) | (~z)))

/* ROTATE_LEFT rotates x left n bits. */
#define ROTATE_LEFT(x, n) (((x) << (n)) | ((x) >> (32-(n))))

/* FF, GG, HH, and II transformations for rounds 1, 2, 3, and 4.
   Rotation is separate from addition to prevent recomputation. */
#define FF(a, b, c, d, x, s, ac) {\
	(a) += F ((b), (c), (d)) + (x) + (UINT4)(ac);\
	(a) = ROTATE_LEFT ((a), (s));\
	(a) += (b);\
	}

#define GG(a, b, c, d, x, s, ac) {\
	(a) += G ((b), (c), (d)) + (x) + (UINT4)(ac);\
	(a) = ROTATE_LEFT ((a), (s));\
	(a) += (b);\
	}

#define HH(a, b, c, d, x, s, ac) {\
	(a) += H ((b), (c), (d)) + (x) + (UINT4)(ac);\
	(a) = ROTATE_LEFT ((a), (s));\
	(a) += (b);\
	}

#define II(a, b, c, d, x, s, ac) {\
	(a) += I ((b), (c), (d)) + (x) + (UINT4)(ac);\
	(a) = ROTATE_LEFT ((a), (s));\
	(a) += (b);\
	}

void MD5Init(MD5_CTX* context);
void MD5Update(MD5_CTX* context, POINTER input, unsigned int inputLen);  
void MD5Final (unsigned char digest[16], MD5_CTX* context);

void MD5Transform(UINT4 state[4], unsigned char block[64]);
void Encode(POINTER output, UINT4* input, unsigned int len);
void Decode(UINT4* output, POINTER input, unsigned int len); 
void MD5_memcpy(POINTER output, POINTER input, unsigned int len); 
void MD5_memset(POINTER output, int value, unsigned int len); 

#endif
