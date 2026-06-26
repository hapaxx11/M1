/* See COPYING.txt for license details (this project is GPLv3). */

/*
 * mfkey32.c - On-device MIFARE Classic Crypto-1 key recovery (mfkey32v2)
 *
 * Self-contained, memory-bounded port of the Crapto-1 recovery. Ported from
 * noproto/FlipperMfkey (GPLv3) which runs in ~110 KB by slicing the state
 * recovery across the most-significant byte (MSB_LIMIT chunks). Crapto-1
 * lineage: bla / Karsten Nohl / Courtois et al. This project is GPLv3, so the
 * port is license-compatible. Attribution preserved.
 *
 * Deliberately does NOT use mfc_crypto1.c (a non-standard cipher reimpl). All
 * cipher/recovery math here is canonical and validated by mfkey32_selftest()
 * against a published known-answer vector.
 *
 * Memory: runs out of a fixed file-scope arena (~110 KB .bss, no malloc, no
 * FreeRTOS heap use) so the footprint is deterministic and isolated.
 *
 * Build the host self-test on a PC with:
 *   cc -O2 -DMFKEY32_HOST_TEST mfkey32.c -o /tmp/mfkey32_test && /tmp/mfkey32_test
 */

#include "mfkey32.h"
#include <string.h>

/* ----- canonical Crapto-1 primitives (from FlipperMfkey crypto1.h) ------- */

#define LF_POLY_ODD  (0x29CE5C)
#define LF_POLY_EVEN (0x870804)
#define BIT(x, n)    ((x) >> (n) & 1)
#define BEBIT(x, n)  BIT(x, (n) ^ 24)
#define SWAPENDIAN(x) \
    ((x) = ((x) >> 8 & 0xff00ff) | ((x) & 0xff00ff) << 8, (x) = (x) >> 16 | (x) << 16)

#define CONST_M1_1 (LF_POLY_EVEN << 1 | 1)
#define CONST_M2_1 (LF_POLY_ODD << 1)
#define CONST_M1_2 (LF_POLY_ODD)
#define CONST_M2_2 (LF_POLY_EVEN << 1 | 1)

#define MSB_LIMIT 16  /* chunk size out of 256; arena below is sized for this */

struct Crypto1State {
    uint32_t odd, even;
};

struct Msb {
    int tail;
    uint32_t states[768];
};

/* Minimal mfkey32 nonce: two captured auth attempts on the same key. */
typedef struct {
    uint32_t uid;
    uint32_t nt0, nt1;
    uint32_t uid_xor_nt0, uid_xor_nt1;
    uint32_t p64, p64b;          /* 64th PRNG successor of nt0 / nt1 */
    uint32_t nr0_enc, ar0_enc;   /* first  encrypted reader challenge / answer */
    uint32_t nr1_enc, ar1_enc;   /* second encrypted reader challenge / answer */
    uint64_t key;                /* output: recovered 48-bit key */
} mfkey32_nonce_t;

static const uint8_t lookup1[256] = {
    0, 0,  16, 16, 0,  16, 0,  0,  0, 16, 0,  0,  16, 16, 16, 16, 0, 0,  16, 16, 0,  16, 0,  0,
    0, 16, 0,  0,  16, 16, 16, 16, 0, 0,  16, 16, 0,  16, 0,  0,  0, 16, 0,  0,  16, 16, 16, 16,
    8, 8,  24, 24, 8,  24, 8,  8,  8, 24, 8,  8,  24, 24, 24, 24, 8, 8,  24, 24, 8,  24, 8,  8,
    8, 24, 8,  8,  24, 24, 24, 24, 8, 8,  24, 24, 8,  24, 8,  8,  8, 24, 8,  8,  24, 24, 24, 24,
    0, 0,  16, 16, 0,  16, 0,  0,  0, 16, 0,  0,  16, 16, 16, 16, 0, 0,  16, 16, 0,  16, 0,  0,
    0, 16, 0,  0,  16, 16, 16, 16, 8, 8,  24, 24, 8,  24, 8,  8,  8, 24, 8,  8,  24, 24, 24, 24,
    0, 0,  16, 16, 0,  16, 0,  0,  0, 16, 0,  0,  16, 16, 16, 16, 0, 0,  16, 16, 0,  16, 0,  0,
    0, 16, 0,  0,  16, 16, 16, 16, 8, 8,  24, 24, 8,  24, 8,  8,  8, 24, 8,  8,  24, 24, 24, 24,
    8, 8,  24, 24, 8,  24, 8,  8,  8, 24, 8,  8,  24, 24, 24, 24, 0, 0,  16, 16, 0,  16, 0,  0,
    0, 16, 0,  0,  16, 16, 16, 16, 8, 8,  24, 24, 8,  24, 8,  8,  8, 24, 8,  8,  24, 24, 24, 24,
    8, 8,  24, 24, 8,  24, 8,  8,  8, 24, 8,  8,  24, 24, 24, 24};
static const uint8_t lookup2[256] = {
    0, 0, 4, 4, 0, 4, 0, 0, 0, 4, 0, 0, 4, 4, 4, 4, 0, 0, 4, 4, 0, 4, 0, 0, 0, 4, 0, 0, 4,
    4, 4, 4, 2, 2, 6, 6, 2, 6, 2, 2, 2, 6, 2, 2, 6, 6, 6, 6, 2, 2, 6, 6, 2, 6, 2, 2, 2, 6,
    2, 2, 6, 6, 6, 6, 0, 0, 4, 4, 0, 4, 0, 0, 0, 4, 0, 0, 4, 4, 4, 4, 2, 2, 6, 6, 2, 6, 2,
    2, 2, 6, 2, 2, 6, 6, 6, 6, 0, 0, 4, 4, 0, 4, 0, 0, 0, 4, 0, 0, 4, 4, 4, 4, 0, 0, 4, 4,
    0, 4, 0, 0, 0, 4, 0, 0, 4, 4, 4, 4, 0, 0, 4, 4, 0, 4, 0, 0, 0, 4, 0, 0, 4, 4, 4, 4, 2,
    2, 6, 6, 2, 6, 2, 2, 2, 6, 2, 2, 6, 6, 6, 6, 0, 0, 4, 4, 0, 4, 0, 0, 0, 4, 0, 0, 4, 4,
    4, 4, 0, 0, 4, 4, 0, 4, 0, 0, 0, 4, 0, 0, 4, 4, 4, 4, 2, 2, 6, 6, 2, 6, 2, 2, 2, 6, 2,
    2, 6, 6, 6, 6, 2, 2, 6, 6, 2, 6, 2, 2, 2, 6, 2, 2, 6, 6, 6, 6, 2, 2, 6, 6, 2, 6, 2, 2,
    2, 6, 2, 2, 6, 6, 6, 6, 2, 2, 6, 6, 2, 6, 2, 2, 2, 6, 2, 2, 6, 6, 6, 6};

static inline int filter(uint32_t const x) {
    uint32_t f;
    f = lookup1[x & 0xff] | lookup2[(x >> 8) & 0xff];
    f |= 0x0d938 >> (x >> 16 & 0xf) & 1;
    return BIT(0xEC57E80A, f);
}

static inline uint8_t evenparity32(uint32_t x) {
    return (uint8_t)__builtin_parity(x);
}

static inline void update_contribution(unsigned int data[], int item, int mask1, int mask2) {
    int p = data[item] >> 25;
    p = p << 1 | evenparity32(data[item] & mask1);
    p = p << 1 | evenparity32(data[item] & mask2);
    data[item] = p << 24 | (data[item] & 0xffffff);
}

static inline uint32_t crypt_word(struct Crypto1State* s) {
    uint32_t res_ret = 0;
    uint32_t feedin, t;
    for(int i = 0; i <= 31; i++) {
        res_ret |= (filter(s->odd) << (24 ^ i));
        feedin = LF_POLY_EVEN & s->even;
        feedin ^= LF_POLY_ODD & s->odd;
        s->even = s->even << 1 | (evenparity32(feedin));
        t = s->odd, s->odd = s->even, s->even = t;
    }
    return res_ret;
}

static inline void crypt_word_noret(struct Crypto1State* s, uint32_t in, int x) {
    uint8_t ret;
    uint32_t feedin, t, next_in;
    for(int i = 0; i <= 31; i++) {
        next_in = BEBIT(in, i);
        ret = filter(s->odd);
        feedin = ret & (!!x);
        feedin ^= LF_POLY_EVEN & s->even;
        feedin ^= LF_POLY_ODD & s->odd;
        feedin ^= !!next_in;
        s->even = s->even << 1 | (evenparity32(feedin));
        t = s->odd, s->odd = s->even, s->even = t;
    }
}

static inline void rollback_word_noret(struct Crypto1State* s, uint32_t in, int x) {
    uint8_t ret;
    uint32_t feedin, t, next_in;
    for(int i = 31; i >= 0; i--) {
        next_in = BEBIT(in, i);
        s->odd &= 0xffffff;
        t = s->odd, s->odd = s->even, s->even = t;
        ret = filter(s->odd);
        feedin = ret & (!!x);
        feedin ^= s->even & 1;
        feedin ^= LF_POLY_EVEN & (s->even >>= 1);
        feedin ^= LF_POLY_ODD & s->odd;
        feedin ^= !!next_in;
        s->even |= (evenparity32(feedin)) << 23;
    }
}

static inline uint8_t napi_lfsr_rollback_bit(struct Crypto1State* s, uint32_t in, int fb) {
    int out;
    uint8_t ret;
    uint32_t t;
    s->odd &= 0xffffff;
    t = s->odd, s->odd = s->even, s->even = t;

    out = s->even & 1;
    out ^= LF_POLY_EVEN & (s->even >>= 1);
    out ^= LF_POLY_ODD & s->odd;
    out ^= !!in;
    out ^= (ret = filter(s->odd)) & !!fb;

    s->even |= evenparity32(out) << 23;
    return ret;
}

static inline uint32_t napi_lfsr_rollback_word(struct Crypto1State* s, uint32_t in, int fb) {
    int i;
    uint32_t ret = 0;
    for(i = 31; i >= 0; --i)
        ret |= napi_lfsr_rollback_bit(s, BEBIT(in, i), fb) << (i ^ 24);
    return ret;
}

static inline uint32_t prng_successor(uint32_t x, uint32_t n) {
    SWAPENDIAN(x);
    while(n--) x = x >> 1 | (x >> 16 ^ x >> 18 ^ x >> 19 ^ x >> 21) << 31;
    return SWAPENDIAN(x);
}

static void crypto1_get_lfsr(struct Crypto1State* state, uint64_t* lfsr) {
    int i;
    uint64_t lfsr_value = 0;
    for(i = 23; i >= 0; --i) {
        lfsr_value = lfsr_value << 1 | BIT(state->odd, i ^ 3);
        lfsr_value = lfsr_value << 1 | BIT(state->even, i ^ 3);
    }
    *lfsr = lfsr_value;
}

/* ----- recovery (from FlipperMfkey mfkey.c, mfkey32 path only) ----------- */

/* fixed working arena (~110 KB, .bss) - no malloc, deterministic footprint */
static struct Msb   s_odd_msbs[MSB_LIMIT];
static struct Msb   s_even_msbs[MSB_LIMIT];
static unsigned int s_temp_states_odd[1280];
static unsigned int s_temp_states_even[1280];
static unsigned int s_states_buffer[1024];

/* tick hook, set per-recovery; lets the caller feed the watchdog / cancel */
static mfkey32_tick_cb s_tick_cb = 0;
static void*           s_tick_ctx = 0;
static volatile int    s_abort = 0;

static int check_state(struct Crypto1State* t, mfkey32_nonce_t* n) {
    if(!(t->odd | t->even)) return 0;
    uint32_t rb = (napi_lfsr_rollback_word(t, 0, 0) ^ n->p64);
    if(rb != n->ar0_enc) {
        return 0;
    }
    rollback_word_noret(t, n->nr0_enc, 1);
    rollback_word_noret(t, n->uid_xor_nt0, 0);
    struct Crypto1State temp = {t->odd, t->even};
    crypt_word_noret(t, n->uid_xor_nt1, 0);
    crypt_word_noret(t, n->nr1_enc, 1);
    if(n->ar1_enc == (crypt_word(t) ^ n->p64b)) {
        crypto1_get_lfsr(&temp, &(n->key));
        return 1;
    }
    return 0;
}

static inline int state_loop(
    unsigned int* states_buffer,
    int xks,
    int m1,
    int m2,
    unsigned int in,
    uint8_t and_val) {
    int states_tail = 0;
    int round = 0, s = 0, xks_bit = 0, round_in = 0;

    for(round = 1; round <= 12; round++) {
        xks_bit = BIT(xks, round);
        if(round > 4) {
            round_in = ((in >> (2 * (round - 4))) & and_val) << 24;
        }

        for(s = 0; s <= states_tail; s++) {
            states_buffer[s] <<= 1;

            if((filter(states_buffer[s]) ^ filter(states_buffer[s] | 1)) != 0) {
                states_buffer[s] |= filter(states_buffer[s]) ^ xks_bit;
                if(round > 4) {
                    update_contribution(states_buffer, s, m1, m2);
                    states_buffer[s] ^= round_in;
                }
            } else if(filter(states_buffer[s]) == xks_bit) {
                if(round > 4) {
                    states_buffer[++states_tail] = states_buffer[s + 1];
                    states_buffer[s + 1] = states_buffer[s] | 1;
                    update_contribution(states_buffer, s, m1, m2);
                    states_buffer[s++] ^= round_in;
                    update_contribution(states_buffer, s, m1, m2);
                    states_buffer[s] ^= round_in;
                } else {
                    states_buffer[++states_tail] = states_buffer[++s];
                    states_buffer[s] = states_buffer[s - 1] | 1;
                }
            } else {
                states_buffer[s--] = states_buffer[states_tail--];
            }
        }
    }

    return states_tail;
}

static int binsearch(unsigned int data[], int start, int stop) {
    int mid, val = data[stop] & 0xff000000;
    while(start != stop) {
        mid = (stop - start) >> 1;
        if((data[start + mid] ^ 0x80000000) > ((unsigned int)val ^ 0x80000000))
            stop = start + mid;
        else
            start += mid + 1;
    }
    return start;
}

static void quicksort(unsigned int array[], int low, int high) {
    if(low >= high) return;
    int middle = low + (high - low) / 2;
    unsigned int pivot = array[middle];
    int i = low, j = high;
    while(i <= j) {
        while(array[i] < pivot) {
            i++;
        }
        while(array[j] > pivot) {
            j--;
        }
        if(i <= j) {
            unsigned int temp = array[i];
            array[i] = array[j];
            array[j] = temp;
            i++;
            j--;
        }
    }
    if(low < j) {
        quicksort(array, low, j);
    }
    if(high > i) {
        quicksort(array, i, high);
    }
}

static int extend_table(
    unsigned int data[], int tbl, int end, int bit, int m1, int m2, unsigned int in) {
    in <<= 24;
    for(data[tbl] <<= 1; tbl <= end; data[++tbl] <<= 1) {
        if((filter(data[tbl]) ^ filter(data[tbl] | 1)) != 0) {
            data[tbl] |= filter(data[tbl]) ^ bit;
            update_contribution(data, tbl, m1, m2);
            data[tbl] ^= in;
        } else if(filter(data[tbl]) == bit) {
            data[++end] = data[tbl + 1];
            data[tbl + 1] = data[tbl] | 1;
            update_contribution(data, tbl, m1, m2);
            data[tbl++] ^= in;
            update_contribution(data, tbl, m1, m2);
            data[tbl] ^= in;
        } else {
            data[tbl--] = data[end--];
        }
    }
    return end;
}

static int old_recover(
    unsigned int odd[], int o_head, int o_tail, int oks,
    unsigned int even[], int e_head, int e_tail, int eks,
    int rem, int s, mfkey32_nonce_t* n, unsigned int in, int first_run) {
    int o, e, i;
    if(rem == -1) {
        for(e = e_head; e <= e_tail; ++e) {
            even[e] = (even[e] << 1) ^ evenparity32(even[e] & LF_POLY_EVEN) ^ (!!(in & 4));
            for(o = o_head; o <= o_tail; ++o, ++s) {
                struct Crypto1State temp = {0, 0};
                temp.even = odd[o];
                temp.odd = even[e] ^ evenparity32(odd[o] & LF_POLY_ODD);
                if(check_state(&temp, n)) {
                    return -1;
                }
            }
        }
        return s;
    }
    if(first_run == 0) {
        for(i = 0; (i < 4) && (rem-- != 0); i++) {
            oks >>= 1;
            eks >>= 1;
            in >>= 2;
            o_tail = extend_table(
                odd, o_head, o_tail, oks & 1, LF_POLY_EVEN << 1 | 1, LF_POLY_ODD << 1, 0);
            if(o_head > o_tail) return s;
            e_tail = extend_table(
                even, e_head, e_tail, eks & 1, LF_POLY_ODD, LF_POLY_EVEN << 1 | 1, in & 3);
            if(e_head > e_tail) return s;
        }
    }
    first_run = 0;
    quicksort(odd, o_head, o_tail);
    quicksort(even, e_head, e_tail);
    while(o_tail >= o_head && e_tail >= e_head) {
        if(((odd[o_tail] ^ even[e_tail]) >> 24) == 0) {
            o_tail = binsearch(odd, o_head, o = o_tail);
            e_tail = binsearch(even, e_head, e = e_tail);
            s = old_recover(
                odd, o_tail--, o, oks, even, e_tail--, e, eks, rem, s, n, in, first_run);
            if(s == -1) {
                break;
            }
        } else if((odd[o_tail] ^ 0x80000000) > (even[e_tail] ^ 0x80000000)) {
            o_tail = binsearch(odd, o_head, o_tail) - 1;
        } else {
            e_tail = binsearch(even, e_head, e_tail) - 1;
        }
    }
    return s;
}

/* Process one MSB chunk. Returns 1 if a valid key was found and stored in n. */
static int calculate_msb_tables(
    int oks, int eks, int msb_round, mfkey32_nonce_t* n,
    unsigned int* states_buffer, struct Msb* odd_msbs, struct Msb* even_msbs,
    unsigned int* temp_states_odd, unsigned int* temp_states_even, unsigned int in) {
    unsigned int msb_head = (MSB_LIMIT * msb_round);
    unsigned int msb_tail = (MSB_LIMIT * (msb_round + 1));
    int states_tail = 0, tail = 0;
    int i = 0, j = 0, semi_state = 0, found = 0;
    unsigned int msb = 0;
    in = ((in >> 16 & 0xff) | (in << 16) | (in & 0xff00)) << 1;
    memset(odd_msbs, 0, MSB_LIMIT * sizeof(struct Msb));
    memset(even_msbs, 0, MSB_LIMIT * sizeof(struct Msb));

    for(semi_state = 1 << 20; semi_state >= 0; semi_state--) {
        if((semi_state & 0x7fff) == 0) {
            if(s_tick_cb && !s_tick_cb(msb_round, s_tick_ctx)) {
                s_abort = 1;
                return 0;
            }
        }
        if(filter(semi_state) == (oks & 1)) {
            states_buffer[0] = semi_state;
            states_tail = state_loop(states_buffer, oks, CONST_M1_1, CONST_M2_1, 0, 0);

            for(i = states_tail; i >= 0; i--) {
                msb = states_buffer[i] >> 24;
                if((msb >= msb_head) && (msb < msb_tail)) {
                    found = 0;
                    for(j = 0; j < odd_msbs[msb - msb_head].tail - 1; j++) {
                        if(odd_msbs[msb - msb_head].states[j] == states_buffer[i]) {
                            found = 1;
                            break;
                        }
                    }
                    if(!found) {
                        tail = odd_msbs[msb - msb_head].tail++;
                        odd_msbs[msb - msb_head].states[tail] = states_buffer[i];
                    }
                }
            }
        }

        if(filter(semi_state) == (eks & 1)) {
            states_buffer[0] = semi_state;
            states_tail = state_loop(states_buffer, eks, CONST_M1_2, CONST_M2_2, in, 3);

            for(i = 0; i <= states_tail; i++) {
                msb = states_buffer[i] >> 24;
                if((msb >= msb_head) && (msb < msb_tail)) {
                    found = 0;
                    for(j = 0; j < even_msbs[msb - msb_head].tail; j++) {
                        if(even_msbs[msb - msb_head].states[j] == states_buffer[i]) {
                            found = 1;
                            break;
                        }
                    }
                    if(!found) {
                        tail = even_msbs[msb - msb_head].tail++;
                        even_msbs[msb - msb_head].states[tail] = states_buffer[i];
                    }
                }
            }
        }
    }

    oks >>= 12;
    eks >>= 12;

    for(i = 0; i < MSB_LIMIT; i++) {
        memset(temp_states_even, 0, sizeof(unsigned int) * (1280));
        memset(temp_states_odd, 0, sizeof(unsigned int) * (1280));
        memcpy(temp_states_odd, odd_msbs[i].states, odd_msbs[i].tail * sizeof(unsigned int));
        memcpy(temp_states_even, even_msbs[i].states, even_msbs[i].tail * sizeof(unsigned int));
        int res = old_recover(
            temp_states_odd, 0, odd_msbs[i].tail, oks,
            temp_states_even, 0, even_msbs[i].tail, eks,
            3, 0, n, in >> 16, 1);
        if(res == -1) {
            return 1;
        }
    }
    return 0;
}

/* ----- public API -------------------------------------------------------- */

bool mfkey32v2_recover(uint32_t uid,
                       uint32_t nt0, uint32_t nr0, uint32_t ar0,
                       uint32_t nt1, uint32_t nr1, uint32_t ar1,
                       uint64_t *key_out,
                       mfkey32_tick_cb tick, void *tick_ctx) {
    mfkey32_nonce_t n;
    s_tick_cb = tick;
    s_tick_ctx = tick_ctx;
    s_abort = 0;
    n.uid = uid;
    n.nt0 = nt0;
    n.nt1 = nt1;
    n.uid_xor_nt0 = uid ^ nt0;
    n.uid_xor_nt1 = uid ^ nt1;
    n.p64 = prng_successor(nt0, 64);
    n.p64b = prng_successor(nt1, 64);
    n.nr0_enc = nr0;
    n.ar0_enc = ar0;
    n.nr1_enc = nr1;
    n.ar1_enc = ar1;
    n.key = 0;

    int ks2 = (int)(ar0 ^ n.p64);
    unsigned int in = 0;
    int oks = 0, eks = 0, i, msb;
    for(i = 31; i >= 0; i -= 2) oks = oks << 1 | BEBIT(ks2, i);
    for(i = 30; i >= 0; i -= 2) eks = eks << 1 | BEBIT(ks2, i);

    for(msb = 0; msb <= ((256 / MSB_LIMIT) - 1); msb++) {
        if(calculate_msb_tables(
               oks, eks, msb, &n, s_states_buffer, s_odd_msbs, s_even_msbs,
               s_temp_states_odd, s_temp_states_even, in)) {
            if(key_out) *key_out = n.key;
            return true;
        }
        if(s_abort) {
            return false;
        }
    }
    return false;
}

bool mfkey32_selftest(void) {
    /* Published known-answer vector (mfkey32v2):
     * uid 2a234f80, nt0 240bd022, {nr0} ad2e1687, {ar0} 57e6f7e4,
     * nt1 18a4bd3e, {nr1} accc1a23, {ar1} 6f10e401  ->  key a0a1a2a3a4a5 */
    uint64_t key = 0;
    bool ok = mfkey32v2_recover(
        0x2a234f80u, 0x240bd022u, 0xad2e1687u, 0x57e6f7e4u,
        0x18a4bd3eu, 0xaccc1a23u, 0x6f10e401u, &key, 0, 0);
    return ok && (key == 0xa0a1a2a3a4a5ULL);
}

#ifdef MFKEY32_HOST_TEST
#include <stdio.h>
int main(void) {
    uint64_t key = 0;
    bool ok = mfkey32v2_recover(
        0x2a234f80u, 0x240bd022u, 0xad2e1687u, 0x57e6f7e4u,
        0x18a4bd3eu, 0xaccc1a23u, 0x6f10e401u, &key, 0, 0);
    printf("recover: ok=%d key=%012llx (expect a0a1a2a3a4a5)\n",
           ok, (unsigned long long)key);
    printf("selftest: %s\n", mfkey32_selftest() ? "PASS" : "FAIL");
    size_t arena = sizeof(s_odd_msbs) + sizeof(s_even_msbs) +
                   sizeof(s_temp_states_odd) + sizeof(s_temp_states_even) +
                   sizeof(s_states_buffer);
    printf("static arena: %zu bytes (%.1f KB)\n", arena, arena / 1024.0);
    return mfkey32_selftest() ? 0 : 1;
}
#endif
