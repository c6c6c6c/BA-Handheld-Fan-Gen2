
#ifndef _XXTEA_H_
#define _XXTEA_H_

#include <stdint.h>

void xxtea_encrypt(uint32_t *v, uint32_t n, const uint32_t key[4]);
void xxtea_decrypt(uint32_t *v, uint32_t n, const uint32_t key[4]);

#endif
