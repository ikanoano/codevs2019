#include <stdint.h>
#include <string.h>

#define NELEMS(x) ((int)(sizeof(x) / sizeof((x)[0])))
#define DEBUG(...) fprintf(stderr, __VA_ARGS__)
#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))
#ifndef TURN_END
#define TURN_END  500
#endif

typedef union {
  uint32_t  raw; //{b[3], b[2], b[1], b[0]}
  uint8_t   b[4];
  /* b[0] b[1]
   * b[3] b[2] */
} pack_t;

static inline pack_t rotate(pack_t pack, int rotnum) {
  pack_t rp;
  rp.raw = (pack.raw << (rotnum*8)) | (pack.raw >> ((4-rotnum)*8));
  return rp;
}

typedef struct {
  int       turn_num;
  int       time_left;
  int       ojama_left;
  int       skill_charge;
  uint64_t  field[18];
  uint8_t   top[10];  // this top points empty space
} player_state_t;

static inline int get_field(uint64_t field[18], int y, int x) {
  return (field[y] >> (5*x)) & 0x1Ful;
}
static inline uint64_t is_filled_field(player_state_t *s, int y, int x) {
  return s->top[x] > y;
}
static inline int isfatal(const player_state_t *s) {
  return s->field[16]!=0 || s->turn_num>=500;
}

typedef struct {
  int32_t        from;
  int32_t        to;
} fromto_t; // closed interval

const fromto_t NOFALL = {18, -1};

static inline int is_nofall(fromto_t *ft) {
  return memcmp(ft, &NOFALL, sizeof(fromto_t))==0;
}

typedef struct {
  uint16_t        offset;
  uint16_t        rotnum;
  int             score;
} search_t;

