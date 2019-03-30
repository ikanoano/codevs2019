#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <limits.h>

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

static int        chain_ojama[64];
static int        skill_ojama[192];
static pack_t     packs[500];
static FILE       *logger;

void sanitize_end() {
  char end[4] = {'\0'};
  scanf("%3s%*[^\n]", end);
  if(strcmp(end, "END")) {DEBUG("end?\n"); exit(1);}
}

void dump_field(int turn, uint64_t field[18]) {
  fprintf(logger, "turn %3d\n", turn);
  fprintf(logger, "   9876543210\n");
  for (int y = 17; y >= 0; y--) {
    fprintf(logger, "%2d|", y);
    for (int x = 9; x >= 0; x--) {
      int val = (field[y]>>(5*x)) & 0x1Ful;
      char c =
        val ==  0 ? '_' :
        val == 11 ? '#' :
                    val+'0';
      fprintf(logger, "%c", c);
    }
    fprintf(logger, "\n");
  }
  fprintf(logger, "\n");
  fflush(logger);
}

void init() {
  srand(13);

  int chain_score = 0;
  chain_ojama[0] = 0;
  for (int i = 1; i < NELEMS(chain_ojama); i++) {
    chain_score += (int)pow(1.3, i);
    chain_ojama[i] = chain_score/2;
    //DEBUG("%3d chain = %8d ojama\n", i, chain_ojama[i]);
  }

  skill_ojama[0] = 0;
  for (int i = 1; i < NELEMS(skill_ojama); i++) {
    int skill_score = (int)(25.0*pow(2, i/12.0));
    skill_ojama[i] = skill_score/2;
    //DEBUG("%3d  bomb = %8d ojama\n", i, skill_ojama[i]);
  }

  logger = fopen("/tmp/aaikiso_log", "w");
  if(logger == NULL) {DEBUG("error logger\n"); exit(1);}
}

void init_input() {
  for (int i = 0; i < 500; i++) {
    int block[4];
    scanf("%d %d", &block[0], &block[1]);
    scanf("%d %d", &block[2], &block[3]);
    packs[i].b[0] = block[0];
    packs[i].b[1] = block[1];
    packs[i].b[3] = block[2];
    packs[i].b[2] = block[3];

    sanitize_end();
    //DEBUG("%08x\n", packs[i].raw);
  }
}

typedef struct {
  int       turn_num;
  int       time_left;
  int       ojama_left;
  int       skill_charge;
  uint64_t  field[18];
} player_state_t;
static inline int get_field(uint64_t field[18], int y, int x) {
  return (field[y] >> (5*x)) & 0x1Ful;
}

void turn_input_player(player_state_t *s) {
  scanf("%d", &s->time_left);
  scanf("%d", &s->ojama_left);
  scanf("%d", &s->skill_charge);

  s->field[17] = s->field[16] = 0;
  for (int i = 15; i >= 0; i--) {
    int block[10];
    scanf("%d %d %d %d %d %d %d %d %d %d",
        &block[9], &block[8], &block[7], &block[6], &block[5],
        &block[4], &block[3], &block[2], &block[1], &block[0]);
    uint64_t row = 0;
    for (int j = 0; j < 10 ; j++) row |= (uint64_t)block[j] << (5*j);
    s->field[i] = row;
  }
  sanitize_end();
}

void turn_input(player_state_t *me, player_state_t *rival) {
  int turn_num;
  scanf("%d", &turn_num);
  me->turn_num = rival->turn_num = turn_num;
  turn_input_player(me);
  turn_input_player(rival);
}

typedef struct {
  int32_t        from;
  int32_t        to;
} fromto_t; // closed interval
const fromto_t NOFALL = {18, -1};
fromto_t fall(uint64_t field[18]) {
  fromto_t ft = NOFALL;
  for (int x = 0; x < 10; x++) {
    int top = 0;
    for (int y = 0; y < 18; y++) {
      uint64_t mask   = 0x1Ful << (5*x);
      uint64_t focus  = field[y] & mask;
      if(!focus) continue;

      if(top!=y) { // fall
        field[y  ] &= ~mask;
        assert((field[top] & mask) == 0); //field[top] &= ~mask;
        field[top] |= focus;
        if(ft.from > top) ft.from = top;
        if(ft.to   < y)   ft.to   = y;
      }
      top++;
    }
  }
  return ft;
}

int vanish(uint64_t field[18], fromto_t ft) {
  if(memcmp(&ft, &NOFALL, sizeof(fromto_t))==0) return 0;
  assert(ft.from<=ft.to);

  int anyvanish = 0;
  int vcount = 0;

  uint64_t mask_vanish_y0_acc=0;
  for (int y = MAX(ft.from-1,0); y < ft.to+1; y++) {
    uint64_t  mask_vanish_y0=0, mask_vanish_y1=0;
    uint64_t  wa0 = (field[y]   ) +        (field[y]  >>5);       // yoko
    uint64_t  wa1 = (field[y]   ) + y<17 ? (field[y+1]   ) : 0;   // tate
    uint64_t  wa2 = (field[y]>>5) + y<17 ? (field[y+1]   ) : 0;   // migiue
    uint64_t  wa3 = (field[y]   ) + y<17 ? (field[y+1]>>5) : 0;   // migishita
    for (int x = 0; x < 10; x++, wa0>>=5, wa1>>=5, wa2>>=5, wa3>>=5) {
      mask_vanish_y1 >>= 5;
      mask_vanish_y0 >>= 5;

      // vanish if sum is 10
      if((wa0 & 0x1F) == 10) { // yoko vanish
        mask_vanish_y0 |= 0x3FFul<<(5* 9);
        anyvanish = 1;
      }
      if((wa1 & 0x1F) == 10) { // tate vanish
        mask_vanish_y1 |=  0x1Ful<<(5* 9);
        mask_vanish_y0 |=  0x1Ful<<(5* 9);
        anyvanish = 1;
      }
      if((wa2 & 0x1F) == 10) { // migiue vanish
        mask_vanish_y1 |=  0x1Ful<<(5* 9);
        mask_vanish_y0 |=  0x1Ful<<(5*10);
        anyvanish = 1;
      }
      if((wa3 & 0x1F) == 10) { // migishita vanish
        mask_vanish_y1 |=  0x1Ful<<(5*10);
        mask_vanish_y0 |=  0x1Ful<<(5* 9);
        anyvanish = 1;
      }
    }
    mask_vanish_y0 |= mask_vanish_y0_acc;
    // execute
    field[y] &= ~mask_vanish_y0;
    // count vanished block
    vcount += __builtin_popcountll(mask_vanish_y0 &
        ((0x21ul<<40) | (0x21ul<<30) | (0x21ul<<20) | (0x21ul<<10) | 0x21ul));
    // accumulate vanished block
    mask_vanish_y0_acc = mask_vanish_y1;
  }
  assert(!anyvanish || vcount>0);
  return vcount;
}

static inline pack_t rotate(pack_t pack, int rotnum) {
  pack_t rp;
  rp.raw = (pack.raw << (rotnum*8)) | (pack.raw >> ((4-rotnum)*8));
  return rp;
}
int drop(int offset, int rotnum, uint64_t field[18], pack_t pack) {
  pack = rotate(pack, rotnum);
  uint64_t mask = (0x3FFul << 5*(8-offset));
  assert((field[17] & mask) == 0);
  assert((field[16] & mask) == 0);
  field[17] |= (((uint64_t)pack.b[0]<<5) | pack.b[1]) << 5*(8-offset);
  field[16] |= (((uint64_t)pack.b[3]<<5) | pack.b[2]) << 5*(8-offset);
  int chain = -1;
  fromto_t ft;
  do {
    ft = fall(field);
    chain++;
  } while(vanish(field, ft));
  return chain;
}

int isfatal(const player_state_t *s) {
  return s->field[16]!=0 || s->turn_num>=500;
}

int static_eval(player_state_t *s) {
  int score = 0;

  // count spaces and blocks around "5" excluding ojama
  uint16_t flag[18] = {0};
  uint16_t ojama[18] = {0};
  uint16_t zero[18] = {0};
  for (int y = 0; y < 18; y++) {
    uint64_t row = s->field[y];
    for (int x = 0; x < 10; x++) {
      int val = row & 0x1F;
      row >>= 5;
      if(val==0)  zero[y]   |= 1<<x;
      if(val==11) ojama[y]  |= 1<<x;
      if(val!=5) continue;
      if(x>0) {flag[y-1] |= 7<<(x-1);}
              {flag[y  ] |= 7<<(x-1);}
      if(x<18){flag[y+1] |= 7<<(x-1);}
    }
  }
  int around5=0, effective5=0;
  for (int y = 0; y < 18; y++) {
    around5     += __builtin_popcount(flag[y] & ~ojama[y]);
    effective5  += __builtin_popcount(flag[y] & ~ojama[y] & ~zero[y]);
  }
  score += skill_ojama[around5]*64;
  score += skill_ojama[effective5]*64;

  return score;
}

int drop_and_eval(player_state_t *s, int offset, int rotnum) {
  int score = 0;
  // drop
  int chain = drop(offset, rotnum, s->field, packs[s->turn_num]);
  score += chain_ojama[chain]*256;

  // fatal
  if(isfatal(s))    return INT_MIN/4;
  // danger
  if(s->field[15])  score -= 1024*1024*4;
  if(s->field[14])  score -= 1024*512;
  if(s->field[13])  score -= 1024*256;

  return score;
}

typedef struct {
  uint16_t        offset;
  uint16_t        rotnum;
  int             score;
} search_t;

int search_comparator_dec(const void *a, const void *b) {
  return ((search_t*)b)->score - ((search_t*)a)->score;
}

search_t search(const player_state_t *s, int recurse_limit) {
  player_state_t  states[9*4];
  search_t        ops[9*4];
  int idx = 0;  // == offset*4+rotnum
  for (int offset = 0; offset < 9; offset++)
  for (int rotnum = 0; rotnum < 4; rotnum++) {
    states[idx] = *s;   // copy field
    int score = drop_and_eval(&states[idx], offset, rotnum);
    score += static_eval(&states[idx]);
    search_t op = {
      .offset=offset,
      .rotnum=rotnum,
      .score=score
    };
    ops[idx] = op;
    states[idx].turn_num++;
    idx++;
  }

  qsort(ops, NELEMS(ops), sizeof(ops[0]), search_comparator_dec);

  if(recurse_limit>0) {
    // travel to only top 5 ops
    const int max_search_op = 5;
    for (int i = 0; i < max_search_op; i++) {
      search_t *op = &ops[i];
      idx = op->offset*4+op->rotnum;
      search_t rslt = search(&states[idx], recurse_limit-1);
      op->score /= recurse_limit+1;
      op->score += rslt.score;
    }
    /*
    for (int i = 0; i < max_search_op; i++) {
      search_t *op = &ops[i];
      idx = op->offset*4+op->rotnum;
      DEBUG("%d(%d) ", idx, op->score);
    }
    DEBUG("\n");
    */
    qsort(ops, max_search_op, sizeof(ops[0]), search_comparator_dec);
  }
  return ops[0];
}

int main(/*int argc, char const* argv[]*/) {
  init();
  printf("aaikiso\n");
  fflush(stdout);

  //uint64_t po[18] = {0};
  //po[17] = 0x040040040040040ul;
  //po[16] = 0x005004003002001ul;
  //po[ 1] = 0x040040040040040ul;
  //po[ 0] = 0x005004003002001ul;
  //dump_field(0,po);

  init_input();

  for (int turn_num = 0; turn_num < TURN_END; turn_num++) {
    player_state_t  me, rival;
    turn_input(&me, &rival);

    if(me.skill_charge>=80) {
      printf("S\n");
    } else {
      search_t best_op = search(&me, 5);
      dump_field(me.turn_num, me.field);
      printf("%d %d\n", best_op.offset, best_op.rotnum);
    }
    fflush(stdout);
  }

  return 0;
}
