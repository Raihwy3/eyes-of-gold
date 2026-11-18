/* First-launch conversion from an exact, byte-order-normalized US cartridge.
 * Only structural offsets are compiled in; source asset bytes come from ROM.
 * All work happens before the original engine or audio threads start. */
#include "native_convert.h"
#include "native_derived.h"
#include "native_inflate.h"
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct ConvertOp {
  uint32_t offset, count, stride, width, repeat, pitch;
};
struct ConvertRow {
  char name[64];
  uint32_t offset, packed, raw, hash, first, ops, compressed, gold;
};
#include "native_convert_recipe.h"
/* Original inflater, called only on bytes from the SHA-1-verified cartridge. */
struct huft {
  uint8_t e, b;
  union {
    uint16_t n;
    struct huft *t;
  } v;
};
extern uint32_t decompressdata(void *, void *, struct huft *);
/* Startup is single-threaded. Keep bulk I/O off libctru's main-thread stack. */
#define CONVERT_IO_BYTES 32768u
static unsigned char convert_io[CONVERT_IO_BYTES];
static char error_detail[160];
static const char *error_text = "Asset conversion failed";
const char *native_convert_error(void) { return error_text; }
static uint32_t hash(uint32_t h, const void *data, size_t n) {
  const unsigned char *p = data;
  while (n--)
    h = (h ^ *p++) * 16777619u;
  return h;
}
static uint32_t le32(const unsigned char *p) {
  return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 |
         (uint32_t)p[3] << 24;
}
static void put32(unsigned char *p, uint32_t n) {
  for (unsigned i = 0; i < 4; i++)
    p[i] = (unsigned char)(n >> (8 * i));
}
static uint32_t be32(const unsigned char *p) {
  return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 |
         p[3];
}
static unsigned be16(const unsigned char *p) {
  return (unsigned)p[0] << 8 | p[1];
}
static int path(char *p, const char *dir, const char *name) {
  int n = snprintf(p, 512, "%s/%s", dir, name);
  return n > 0 && n < 512;
}
static int update(NativeConvertProgress cb, const char *s, unsigned n,
                  unsigned total) {
  if (!cb || cb(s, n, total))
    return 1;
  error_text = "Asset conversion cancelled; launch again to retry";
  return 0;
}
/* Small streaming SHA-1: validate the complete normalized image before inflate.
 */
struct Sha {
  uint32_t h[5];
  uint64_t bytes;
  unsigned used;
  unsigned char block[64];
};
static uint32_t rol(uint32_t v, unsigned n) { return v << n | v >> (32 - n); }
static void sha_block(struct Sha *s) {
  uint32_t w[80], a = s->h[0], b = s->h[1], c = s->h[2], d = s->h[3],
                  e = s->h[4];
  for (unsigned i = 0; i < 80; i++)
    w[i] = i < 16 ? be32(s->block + 4 * i)
                  : rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
  for (unsigned i = 0; i < 80; i++) {
    uint32_t f = i < 20   ? ((b & c) | (~b & d))
                 : i < 40 ? (b ^ c ^ d)
                 : i < 60 ? ((b & c) | (b & d) | (c & d))
                          : (b ^ c ^ d);
    uint32_t k = i < 20   ? 0x5a827999u
                 : i < 40 ? 0x6ed9eba1u
                 : i < 60 ? 0x8f1bbcdcu
                          : 0xca62c1d6u;
    uint32_t t = rol(a, 5) + f + e + k + w[i];
    e = d;
    d = c;
    c = rol(b, 30);
    b = a;
    a = t;
  }
  s->h[0] += a;
  s->h[1] += b;
  s->h[2] += c;
  s->h[3] += d;
  s->h[4] += e;
}
static void sha_feed(struct Sha *s, const void *data, size_t n) {
  const unsigned char *p = data;
  s->bytes += n;
  while (n) {
    unsigned k = 64 - s->used;
    if (k > n)
      k = (unsigned)n;
    memcpy(s->block + s->used, p, k);
    s->used += k;
    p += k;
    n -= k;
    if (s->used == 64) {
      sha_block(s);
      s->used = 0;
    }
  }
}
static int sha_finish(struct Sha *s) {
  uint64_t bits = s->bytes * 8;
  unsigned char pad[72] = {128};
  unsigned n = s->used < 56 ? 56 - s->used : 120 - s->used;
  sha_feed(s, pad, n);
  for (unsigned i = 0; i < 8; i++)
    pad[i] = (unsigned char)(bits >> (56 - 8 * i));
  sha_feed(s, pad, 8);
  const uint32_t expected[] = {0xabe01e4a, 0xeb033b6c, 0x0836819f, 0x549c791b,
                               0x26cfde83};
  return !memcmp(s->h, expected, sizeof expected);
}
static int cart_copy(const char *dir, const char *target,
                     NativeConvertProgress cb) {
  const char *names[] = {"GoldenEye.z64", "GoldenEye.n64", "GoldenEye.v64"};
  char p[512];
  FILE *in = NULL, *out = NULL;
  int ok = 0;
  for (unsigned i = 0; i < 3 && !in; i++)
    if (path(p, dir, names[i]))
      in = fopen(p, "rb");
  if (!in) {
    error_text = "Put the US retail ROM beside the app as GoldenEye.z64, "
                 "GoldenEye.n64 or GoldenEye.v64";
    return 0;
  }
  unsigned char *block = convert_io;
  if (fseek(in, 0, SEEK_END) || ftell(in) != 0xc00000L ||
      fseek(in, 0, SEEK_SET)) {
    error_text = "Wrong ROM size: requires the 12 MiB US retail ROM";
    goto done;
  }
  if (fread(block, 1, 4, in) != 4)
    goto done;
  unsigned order = !memcmp(block, "\x80\x37\x12\x40", 4)   ? 0
                   : !memcmp(block, "\x37\x80\x40\x12", 4) ? 1
                   : !memcmp(block, "\x40\x12\x37\x80", 4) ? 3
                                                           : 2;
  if (order == 2) {
    error_text = "Unrecognized ROM byte order";
    goto done;
  }
  rewind(in);
  out = fopen(target, "wb");
  if (!out) {
    error_text = "Cannot write cartridge cache; check SD space";
    goto done;
  }
  struct Sha sha = {
      {0x67452301u, 0xefcdab89u, 0x98badcfeu, 0x10325476u, 0xc3d2e1f0u},
      0,
      0,
      {0}};
  for (unsigned at = 0; at < 0xc00000u; at += CONVERT_IO_BYTES) {
    if (!update(cb, "Checking ROM", at, 0xc00000u))
      goto done;
    if (fread(block, 1, CONVERT_IO_BYTES, in) != CONVERT_IO_BYTES)
      goto done;
    if (order)
      for (unsigned i = 0; i < CONVERT_IO_BYTES; i += 4) {
        unsigned char t[4];
        memcpy(t, block + i, 4);
        for (unsigned j = 0; j < 4; j++)
          block[i + j] = t[j ^ order];
      }
    sha_feed(&sha, block, CONVERT_IO_BYTES);
    if (fwrite(block, 1, CONVERT_IO_BYTES, out) != CONVERT_IO_BYTES)
      goto done;
  }
  if (!sha_finish(&sha)) {
    error_text = "Unsupported ROM: SHA-1 does not match the US retail release";
    goto done;
  }
  ok = 1;
done:
  if (in)
    fclose(in);
  if (out && fclose(out))
    ok = 0;
  if (!ok)
    remove(target);
  return ok;
}
/* Fixed-Huffman deflate with bounded hash-chain matching. Identical decoded
 * bytes to desktop zlib; compression bytes need not match its heuristics. */
struct Bits {
  unsigned char *p;
  size_t used, capacity;
  uint32_t bits;
  unsigned n;
  int ok;
};
static void bits(struct Bits *b, unsigned v, unsigned n) {
  b->bits |= (uint32_t)v << b->n;
  b->n += n;
  while (b->n >= 8) {
    if (b->used >= b->capacity) {
      b->ok = 0;
      return;
    }
    b->p[b->used++] = (unsigned char)b->bits;
    b->bits >>= 8;
    b->n -= 8;
  }
}
static unsigned reverse(unsigned v, unsigned n) {
  unsigned r = 0;
  while (n--) {
    r = r * 2 + (v & 1);
    v >>= 1;
  }
  return r;
}
static void symbol(struct Bits *b, unsigned s) {
  unsigned n = s < 144 ? 8 : s < 256 ? 9 : s < 280 ? 7 : 8;
  unsigned v = s < 144   ? s + 48
               : s < 256 ? s - 144 + 400
               : s < 280 ? s - 256
                         : s - 280 + 192;
  bits(b, reverse(v, n), n);
}
static unsigned pack(const unsigned char *src, unsigned n, unsigned char *out,
                     unsigned cap) {
  static const unsigned lens[] = {3,  4,  5,  6,   7,   8,   9,   10,  11, 13,
                                  15, 17, 19, 23,  27,  31,  35,  43,  51, 59,
                                  67, 83, 99, 115, 131, 163, 195, 227, 258};
  static const unsigned lex[] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                                 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
  static const unsigned dist[] = {
      1,    2,    3,    4,    5,    7,    9,    13,    17,    25,
      33,   49,   65,   97,   129,  193,  257,  385,   513,   769,
      1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
  int32_t *head = malloc(65536 * sizeof *head),
          *prev = malloc((n ? n : 1) * sizeof *prev);
  if (!head || !prev) {
    free(head);
    free(prev);
    return 0;
  }
  for (unsigned i = 0; i < 65536; i++)
    head[i] = -1;
  out[0] = 0x11;
  out[1] = 0x72;
  struct Bits b = {out, 2, cap, 0, 0, 1};
  bits(&b, 3, 3);
  for (unsigned at = 0; at < n;) {
    unsigned best = 0, distance = 0, h = 0;
    if (at + 2 < n) {
      h = ((unsigned)src[at] * 251u + (unsigned)src[at + 1] * 31u +
           src[at + 2]) &
          65535;
      int32_t p = head[h];
      unsigned steps = 0, limit = n - at < 258 ? n - at : 258;
      while (p >= 0 && at - (unsigned)p <= 32768 && steps++ < 32) {
        unsigned len = 0;
        while (len < limit && src[p + len] == src[at + len])
          len++;
        if (len > best) {
          best = len;
          distance = at - (unsigned)p;
          if (len == limit)
            break;
        }
        p = prev[p];
      }
    }
    unsigned take = best >= 3 ? best : 1;
    if (best >= 3) {
      unsigned l = 0, d = 0;
      while (l < 28 && lens[l + 1] <= best)
        l++;
      symbol(&b, 257 + l);
      bits(&b, best - lens[l], lex[l]);
      while (d < 29 && dist[d + 1] <= distance)
        d++;
      bits(&b, reverse(d, 5), 5);
      unsigned extra = d < 4 ? 0 : d / 2 - 1;
      bits(&b, distance - dist[d], extra);
    } else
      symbol(&b, src[at]);
    for (unsigned j = 0; j < take; j++) {
      unsigned p = at + j;
      if (p + 2 < n) {
        unsigned k = ((unsigned)src[p] * 251u + (unsigned)src[p + 1] * 31u +
                      src[p + 2]) &
                     65535;
        prev[p] = head[k];
        head[k] = (int32_t)p;
      }
    }
    at += take;
  }
  symbol(&b, 256);
  if (b.n)
    bits(&b, 0, 8 - b.n);
  free(head);
  free(prev);
  return b.ok ? (unsigned)b.used : 0;
}
static int game_convert(FILE *rom, const char *target,
                        NativeConvertProgress cb) {
  FILE *out = fopen(target, "wb+");
  if (!out)
    return 0;
  int ok = 0;
  unsigned char *directory = calloc(CONVERT_ROWS, 80);
  struct huft *hlist = calloc(NATIVE_HUFT_ENTRIES, sizeof *hlist);
  if (!directory || !hlist)
    goto done;
  if (fseek(out, 24 + CONVERT_ROWS * 80, SEEK_SET))
    goto done;
  for (unsigned i = 0; i < CONVERT_ROWS; i++) {
    const struct ConvertRow *r = &convert_rows[i];
    unsigned char *src = NULL, *raw = NULL, *packed = NULL;
    unsigned size = 0;
    if (!update(cb, "Converting models and level", i, CONVERT_ROWS))
      goto done;
    /* Retail inflate assumes output precedes input, even for separate buffers:
     * its overlap guard spins forever when nearby heap output follows input.
     * Reserve one extent and put the verified compressed bytes above output. */
    raw = calloc(r->raw + r->packed + 80, 1);
    if (raw)
      src = raw + r->raw + 64;
    packed = malloc(r->raw * 2 + 64);
    if (!src || !raw || !packed) {
      free(raw);
      free(packed);
      goto done;
    }
    int good = !fseek(rom, r->offset, SEEK_SET) &&
               fread(src, 1, r->packed, rom) == r->packed;
    if (good) {
      if (r->compressed)
        good = decompressdata(src, raw, hlist) == r->raw;
      else
        memcpy(raw, src, r->raw);
    }
    if (good)
      for (unsigned j = 0; j < r->ops; j++) {
        const struct ConvertOp *op = &convert_ops[r->first + j];
        for (unsigned entry = 0; entry < op->repeat; entry++)
          for (unsigned k = 0; k < op->count; k++) {
            unsigned at = op->offset + entry * op->pitch + k * op->stride;
            if (at + (op->width ? op->width : 3) > r->raw) {
              good = 0;
              break;
            }
            if (op->width) {
              for (unsigned v = 0; v < op->width / 2; v++) {
                unsigned char t = raw[at + v];
                raw[at + v] = raw[at + op->width - 1 - v];
                raw[at + op->width - 1 - v] = t;
              }
            } else {
              unsigned v = raw[at];
              raw[at + 1] = (unsigned char)((v * 233 + 127) / 255);
              raw[at + 2] = (unsigned char)((v * 119 + 127) / 255);
            }
          }
      }
    if (good)
      good = hash(2166136261u, raw, r->raw) == r->hash;
    if (good) {
      if (r->compressed)
        size = pack(raw, r->raw, packed, r->raw * 2 + 64);
      else {
        memcpy(packed, raw, r->raw);
        size = r->raw;
      }
      good = size != 0;
    }
    if (good) {
      unsigned char *d = directory + 80 * i;
      memcpy(d, r->name, 64);
      long offset = ftell(out);
      good = offset >= 0 && (unsigned long)offset <= UINT32_MAX;
      if (good) {
        put32(d + 64, (uint32_t)offset);
        put32(d + 68, size);
        put32(d + 72, r->raw);
        put32(d + 76, hash(2166136261u, packed, size));
        good = fwrite(packed, 1, size, out) == size;
      }
    }
    free(raw);
    free(packed);
    if (!good) {
      snprintf(error_detail, sizeof error_detail,
               "Model conversion failed: %.63s", r->name);
      error_text = error_detail;
      goto done;
    }
  }
  if (fseek(out, 24, SEEK_SET) ||
      fwrite(directory, 80, CONVERT_ROWS, out) != CONVERT_ROWS || fflush(out) ||
      fseek(out, 24, SEEK_SET))
    goto done;
  uint32_t h = 2166136261u;
  unsigned char *block = convert_io;
  unsigned char header[24];
  size_t n;
  while ((n = fread(block, 1, CONVERT_IO_BYTES, out)))
    h = hash(h, block, n);
  if (ferror(out))
    goto done;
  memcpy(header, "EOGGAME1", 8);
  put32(header + 8, 1);
  put32(header + 12, CONVERT_ROWS);
  put32(header + 16, h);
  put32(header + 20, CONVERT_CART_HASH);
  if (fseek(out, 0, SEEK_SET) || fwrite(header, 1, 24, out) != 24)
    goto done;
  ok = 1;
done:
  free(directory);
  free(hlist);
  if (fclose(out))
    ok = 0;
  if (!ok)
    remove(target);
  return ok;
}
struct PcmBuild {
  unsigned char *books, *records;
  uint32_t *slots;
  unsigned book_count, count, capacity;
};
static int pcm_record(struct PcmBuild *p, unsigned book,
                      const unsigned char *frame, int16_t history[2]) {
  unsigned char r[48] = {0};
  r[0] = (unsigned char)book;
  r[1] = (unsigned char)(book >> 8);
  memcpy(r + 2, history, 4);
  memcpy(r + 6, frame, 9);
  unsigned slot = hash(2166136261u, r, 15) & 524287u;
  while (p->slots[slot]) {
    unsigned char *known = p->records + (p->slots[slot] - 1) * 48;
    if (!memcmp(known, r, 15)) {
      memcpy(history, known + 15 + 28, 4);
      return 1;
    }
    slot = (slot + 1) & 524287u;
  }
  unsigned predictors = le32(p->books + book * 260) / 32, index = frame[0] & 15,
           shift = frame[0] >> 4;
  if (index >= predictors)
    return 0;
  const unsigned char *coeff = p->books + book * 260 + 4 + index * 32;
  int16_t table[16], output[16];
  for (unsigned i = 0; i < 16; i++)
    memcpy(table + i, coeff + 2 * i, 2);
  for (unsigned half = 0; half < 2; half++) {
    int16_t residual[8];
    for (unsigned j = 0; j < 8; j++) {
      unsigned v = (frame[1 + half * 4 + j / 2] >> (j % 2 ? 0 : 4)) & 15;
      int val = v < 8 ? (int)v : (int)v - 16;
      residual[j] = (int16_t)((uint32_t)val << shift);
    }
    int32_t prev2 = half ? output[6] : history[0],
            prev1 = half ? output[7] : history[1];
    for (unsigned j = 0; j < 8; j++) {
      uint32_t acc = (uint32_t)((int32_t)table[j] * prev2) +
                     (uint32_t)((int32_t)table[8 + j] * prev1) +
                     (uint32_t)((int32_t)residual[j] * 2048);
      for (unsigned k = 0; k < j; k++)
        acc += (uint32_t)((int32_t)table[8 + j - k - 1] * residual[k]);
      int32_t signed_acc;
      memcpy(&signed_acc, &acc, 4);
      int32_t value = signed_acc >> 11;
      output[half * 8 + j] = (int16_t)(value < -32768  ? -32768
                                       : value > 32767 ? 32767
                                                       : value);
    }
  }
  memcpy(r + 15, output, 32);
  history[0] = output[14];
  history[1] = output[15];
  if (p->count == p->capacity) {
    unsigned capacity = p->capacity + 16384;
    if (capacity > 300000)
      capacity = 300000;
    if (capacity <= p->count)
      return 0;
    void *data = realloc(p->records, capacity * 48);
    if (!data)
      return 0;
    p->records = data;
    p->capacity = capacity;
  }
  memcpy(p->records + p->count * 48, r, 48);
  p->slots[slot] = ++p->count;
  return 1;
}
static int pcm_compare(const void *a, const void *b) {
  return memcmp(a, b, 15);
}
static int pcm_convert(FILE *rom, const char *target,
                       NativeConvertProgress cb) {
  struct PcmBuild p = {0};
  p.books = calloc(1024, 260);
  p.slots = calloc(524288, 4);
  int ok = 0;
  FILE *out = NULL;
  if (!p.books || !p.slots)
    goto done;
  const unsigned starts[] = {0x2ebde0, 0x3b4450}, ends[] = {0x2f19a0, 0x3b87f0};
  for (unsigned b = 0; b < 2; b++) {
    unsigned size = ends[b] - starts[b];
    unsigned char *bank = malloc(size), *seen = calloc(size, 1);
    if (!bank || !seen) {
      free(bank);
      free(seen);
      goto done;
    }
    if (fseek(rom, starts[b], SEEK_SET) || fread(bank, 1, size, rom) != size) {
      free(bank);
      free(seen);
      goto done;
    }
    /* Offsets are trusted only after complete cartridge SHA-1 verification. */
    for (unsigned bn = 0; bn < be16(bank + 2); bn++) {
      unsigned base = be32(bank + 4 + 4 * bn);
      for (unsigned i = 0; i <= be16(bank + base); i++) {
        unsigned inst = be32(bank + base + 8 + 4 * i);
        if (!inst)
          continue;
        for (unsigned s = 0; s < be16(bank + inst + 14); s++) {
          unsigned sound = be32(bank + inst + 16 + 4 * s),
                   wave = be32(bank + sound + 8);
          if (wave >= size) {
            free(bank);
            free(seen);
            goto done;
          }
          seen[wave] = 1;
        }
      }
    }
    for (unsigned wave = 0; wave < size; wave++)
      if (seen[wave]) {
        if (!update(cb, b ? "Decoding music samples" : "Decoding sound effects",
                    wave, size)) {
          free(bank);
          free(seen);
          goto done;
        }
        unsigned base = be32(bank + wave),
                 length = be32(bank + wave + 4) / 9 * 9, kind = bank[wave + 8];
        if (kind == 1)
          continue;
        unsigned book_at = be32(bank + wave + 16), order = be32(bank + book_at),
                 predictors = be32(bank + book_at + 4);
        if (kind || order != 2 || !predictors || predictors > 8) {
          free(bank);
          free(seen);
          goto done;
        }
        unsigned char book[260] = {0};
        put32(book, 32 * predictors);
        for (unsigned i = 0; i < 16 * predictors; i++) {
          unsigned val = be16(bank + book_at + 8 + 2 * i);
          book[4 + 2 * i] = (unsigned char)val;
          book[5 + 2 * i] = (unsigned char)(val >> 8);
        }
        unsigned id = 0;
        while (id < p.book_count && memcmp(p.books + id * 260, book, 260))
          id++;
        if (id == p.book_count) {
          if (id == 1024) {
            free(bank);
            free(seen);
            goto done;
          }
          memcpy(p.books + id * 260, book, 260);
          p.book_count++;
        }
        unsigned char *data = malloc(length ? length : 1);
        if (!data) {
          free(bank);
          free(seen);
          goto done;
        }
        if (fseek(rom, ends[b] + base, SEEK_SET) ||
            fread(data, 1, length, rom) != length) {
          free(data);
          free(bank);
          free(seen);
          goto done;
        }
        unsigned loop = be32(bank + wave + 12);
        for (unsigned pass = 0; pass < 2; pass++) {
          int16_t history[2] = {0, 0};
          unsigned first = 0;
          if (pass) {
            if (!loop || !be32(bank + loop + 8))
              break;
            unsigned start = be32(bank + loop), end = be32(bank + loop + 4);
            if (start >= end || end > length / 9 * 16) {
              free(data);
              free(bank);
              free(seen);
              goto done;
            }
            first = (start / 16 + 1) * 9;
            history[0] = (int16_t)be16(bank + loop + 12 + 28);
            history[1] = (int16_t)be16(bank + loop + 12 + 30);
          }
          for (unsigned at = first; at < length; at += 9) {
            if ((at / 9) % 4096 == 0 &&
                !update(cb, "Decoding audio frames", p.count, 0)) {
              free(data);
              free(bank);
              free(seen);
              goto done;
            }
            if (!pcm_record(&p, id, data + at, history)) {
              free(data);
              free(bank);
              free(seen);
              goto done;
            }
          }
        }
        free(data);
      }
    free(bank);
    free(seen);
  }
  if (!update(cb, "Saving decoded audio", p.count, p.count))
    goto done;
  qsort(p.records, p.count, 48, pcm_compare);
  unsigned payload = p.book_count * 260 + p.count * 48;
  unsigned char header[32] = {0};
  memcpy(header, "EOGPCM2\0", 8);
  put32(header + 8, 2);
  put32(header + 12, p.book_count);
  put32(header + 16, p.count);
  put32(header + 20, payload);
  uint32_t h = hash(2166136261u, p.books, p.book_count * 260);
  h = hash(h, p.records, p.count * 48);
  put32(header + 24, h);
  out = fopen(target, "wb");
  if (!out)
    goto done;
  ok = fwrite(header, 1, 32, out) == 32 &&
       fwrite(p.books, 260, p.book_count, out) == p.book_count &&
       fwrite(p.records, 48, p.count, out) == p.count;
done:
  if (out && fclose(out))
    ok = 0;
  free(p.books);
  free(p.records);
  free(p.slots);
  if (!ok)
    remove(target);
  return ok;
}
/* Validate caches without allocating the game arena or the live PCM index. */
static int valid_cache(const char *dir, const char *name, unsigned kind) {
  char p[512];
  if (!path(p, dir, name))
    return 0;
  FILE *f = fopen(p, "rb");
  if (!f)
    return 0;
  unsigned char header[32];
  unsigned char *block = convert_io;
  unsigned h = 2166136261u, bytes = 0;
  int ok = 1;
  unsigned header_size = kind == 0 ? 0 : kind == 1 ? 24 : 32;
  if (header_size && fread(header, 1, header_size, f) != header_size)
    ok = 0;
  if (ok && kind == 1)
    ok = !memcmp(header, "EOGGAME1", 8) && le32(header + 8) == 1 &&
         le32(header + 12) == CONVERT_ROWS &&
         le32(header + 20) == CONVERT_CART_HASH;
  if (ok && kind == 1) {
    /* A resource substitution can keep the row count unchanged. Validate the
     * directory against this build's recipe before reusing an older bank. */
    unsigned char row[80];
    for (unsigned i = 0; ok && i < CONVERT_ROWS; ++i)
      ok = fread(row, 1, sizeof(row), f) == sizeof(row) &&
           !memcmp(row, convert_rows[i].name, 64) &&
           le32(row + 72) == convert_rows[i].raw;
    if (ok)
      ok = fseek(f, (long)header_size, SEEK_SET) == 0;
  }
  if (ok && kind == 2)
    ok = !memcmp(header, "EOGPCM2\0", 8) && le32(header + 8) == 2 &&
         !le32(header + 28);
  size_t n;
  while (ok && (n = fread(block, 1, CONVERT_IO_BYTES, f))) {
    h = hash(h, block, n);
    bytes += (unsigned)n;
  }
  ok = ok && !ferror(f);
  if (kind == 0)
    ok = ok && bytes == 0xc00000u && h == CONVERT_CART_HASH;
  else if (kind == 1)
    ok = ok && h == le32(header + 16);
  else
    ok = ok && bytes == le32(header + 20) && h == le32(header + 24) &&
         le32(header + 12) > 0 && le32(header + 12) <= 1024 &&
         (uint64_t)le32(header + 12) * 260 + (uint64_t)le32(header + 16) * 48 ==
             bytes;
  fclose(f);
  return ok;
}
static int commit(const char *dir, const char *temporary, const char *name) {
  char a[512], b[512];
  if (!path(a, dir, temporary) || !path(b, dir, name))
    return 0;
  /* FAT rename cannot replace an existing file. Completed temporary files have
   * already been flushed and closed; a lost final rename is retried next boot.
   */
  remove(b);
  return rename(a, b) == 0;
}
static NativeConvertProgress render_progress;
static int render_update(const char *stage, unsigned done, unsigned total) {
  return update(render_progress, stage, done, total);
}
int native_convert_ensure(const char *dir, NativeConvertProgress cb) {
  error_text = "Asset conversion failed: check free SD space and retry";
  if (!dir || strlen(dir) > 450) {
    error_text = "Asset folder path is too long";
    return 0;
  }
  if (!update(cb, "Checking asset cache", 0, 1))
    return 0;
  int cart = valid_cache(dir, "native-cart.bin", 0),
      game = valid_cache(dir, "native-game.dat", 1),
      pcm = valid_cache(dir, "native-audio.pcm", 2),
      render = native_derived_valid(dir);
  if (cart && game && pcm && render)
    return update(cb, "Assets ready", 1, 1);
  char cartpath[512], datpath[512], pcmpath[512], renderpath[512];
  path(cartpath, dir, "native-cart.bin.tmp");
  path(datpath, dir, "native-game.dat.tmp");
  path(pcmpath, dir, "native-audio.pcm.tmp");
  path(renderpath, dir, "native-render.dat.tmp");
  /* Always authenticate the user's supplied ROM for new conversions. A cached
   * cart is not a substitute for the initial full SHA-1/version check. */
  if (!cart_copy(dir, cartpath, cb))
    return 0;
  FILE *rom = fopen(cartpath, "rb");
  if (!rom) {
    remove(cartpath);
    return 0;
  }
  int ok = (game || game_convert(rom, datpath, cb)) &&
           (pcm || pcm_convert(rom, pcmpath, cb));
  fclose(rom);
  render_progress = cb;
  if (ok && !render)
    ok = native_derived_generate(cartpath, renderpath, render_update);
  if (ok)
    ok = (!game ? commit(dir, "native-game.dat.tmp", "native-game.dat") : 1) &&
         (!pcm ? commit(dir, "native-audio.pcm.tmp", "native-audio.pcm") : 1) &&
         (!render ? commit(dir, "native-render.dat.tmp", "native-render.dat")
                  : 1) &&
         commit(dir, "native-cart.bin.tmp", "native-cart.bin");
  if (!ok) {
    remove(cartpath);
    remove(datpath);
    remove(pcmpath);
    remove(renderpath);
    return 0;
  }
  return update(cb, "Assets ready", 1, 1);
}
