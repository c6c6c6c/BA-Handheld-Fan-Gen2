
#include "xxtea.h"

#define DELTA 0x9E3779B9
#define MX (((z >> 5 ^ y << 2) + (y >> 3 ^ z << 4)) ^ ((sum ^ y) + (key[(p & 3) ^ e] ^ z)))

/**
 * @brief  xxtea encrypt
 * @param  v Data block buffer
 * @param  n Data block length
 * @param  key User's xxtea key
 */
void xxtea_encrypt(uint32_t *v, uint32_t n, const uint32_t key[4]) 
{
    uint32_t y;
    uint32_t z;
    uint32_t sum;
    uint32_t p;
    uint32_t rounds;
    uint32_t e;

    if (n < 1)
    {
        return;
    }

    rounds = 6 + 52 / n;
    sum = 0;
    z = v[n - 1];

    do
    {
        sum += DELTA;
        e = (sum >> 2) & 3;

        for (p = 0; p < n - 1; p++)
        {
            y = v[p + 1];
            z = v[p] += MX;
        }

        y = v[0];
        z = v[n - 1] += MX;
    } while (--rounds);
}

/**
 * @brief  xxtea decrypt
 * @param  v Data block buffer
 * @param  n Data block length
 * @param  key User's xxtea key
 */
void xxtea_decrypt(uint32_t *v, uint32_t n, const uint32_t key[4])
{
    uint32_t y;
    uint32_t z;
    uint32_t sum;
    uint32_t p;
    uint32_t rounds;
    uint32_t e;

    if (n < 1)
    {
        return;
    }

    rounds = 6 + 52 / n;
    sum = rounds * DELTA;
    y = v[0];

    do {
        e = (sum >> 2) & 3;
        for (p = n - 1; p > 0; p--)
        {
            z = v[p - 1];
            y = v[p] -= MX;
        }
        z = v[n - 1];
        y = v[0] -= MX;
        sum -= DELTA;
    } while (--rounds);
}
