#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <limits.h>

#include "ai.h"

static int        chain_ojama[64];
static int        skill_ojama[192];
static pack_t     packs[500];
static FILE       *logger;

void sanitize_end() {
  char end[4] = {'\0'};
  scanf("%3s%*[^\n]", end);
  if(strcmp(end, "END")) {DEBUG("end?\n"); exit(1);}
}

void dump_field(const player_state_t *s) {
  int turn = s->turn_num;
  fprintf(logger, "turn %3d\n", turn);
  fprintf(logger, "   9876543210\n");
  for (int y = 17; y >= 0; y--) {
    fprintf(logger, "%2d|", y);
    for (int x = 9; x >= 0; x--) {
      int val = (s->field[y]>>(5*x)) & 0x1Ful;
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
  logger = fopen("/tmp/aaikiso_log", "w");
  if(logger == NULL) {fprintf(stderr, "error logger\n"); exit(1);}

  srand(13);

  int chain_score = 0;
  chain_ojama[0] = 0;
  for (int i = 1; i < NELEMS(chain_ojama); i++) {
    chain_score += (int)pow(1.3, i);
    chain_ojama[i] = chain_score/2;
    DEBUG("%3d chain = %8d ojama\n", i, chain_ojama[i]);
  }

  skill_ojama[0] = 0;
  for (int i = 1; i < NELEMS(skill_ojama); i++) {
    int skill_score = (int)(25.0*pow(2, i/12.0));
    skill_ojama[i] = skill_score/2;
    DEBUG("%3d  bomb = %8d ojama\n", i, skill_ojama[i]);
  }
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

void turn_input_player(player_state_t *s) {
  scanf("%d", &s->time_left);
  scanf("%d", &s->ojama_left);
  scanf("%d", &s->skill_charge);

  s->field[17] = s->field[16] = 0;
  for (int x = 0; x < 10 ; x++) s->top[x] = 0;
  for (int y = 15; y >= 0; y--) {
    int block[10];
    scanf("%d %d %d %d %d %d %d %d %d %d",
        &block[9], &block[8], &block[7], &block[6], &block[5],
        &block[4], &block[3], &block[2], &block[1], &block[0]);
    uint64_t row = 0;
    for (int x = 0; x < 10 ; x++) {
      if(block[x] && s->top[x]==0) s->top[x] = y+1;
      row |= (uint64_t)block[x] << (5*x);
    }
    s->field[y] = row;
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

fromto_t fall(player_state_t *s) {
  fromto_t ft = NOFALL;
  for (int x = 0; x < 10; x++) {
    int top = 0; // this top points empty
    for (int y = 0; y < 18; y++) {
      uint64_t mask   = 0x1Ful << (5*x);
      uint64_t focus  = s->field[y] & mask;
      if(!focus) continue;

      if(top!=y) { // fall
        s->field[y  ] &= ~mask;
        assert((s->field[top] & mask) == 0); //field[top] &= ~mask;
        s->field[top] |= focus;
        ft.from = MIN(top, ft.from);
        ft.to   = MAX(y,   ft.to);
      }
      top++;
    }
    s->top[x] = top;
  }
  return ft;
}

int vanish(player_state_t *s, fromto_t ft) {
  if(is_nofall(&ft)) return 0;
  assert(ft.from<=ft.to);

  int anyvanish = 0;
  int vcount = 0;

  uint64_t mask_vanish_y0_acc=0;
  for (int y = MAX(ft.from-1,0); y < MIN(ft.to+1, 18); y++) {
    uint64_t  mask_vanish_y0=0, mask_vanish_y1=0;
    uint64_t  wa0 = (s->field[y]   ) +         (s->field[y]  >>5);        // yoko
    uint64_t  wa1 = (s->field[y]   ) + (y<17 ? (s->field[y+1]   ) : 0ul); // tate
    uint64_t  wa2 = (s->field[y]>>5) + (y<17 ? (s->field[y+1]   ) : 0ul); // migiue
    uint64_t  wa3 = (s->field[y]   ) + (y<17 ? (s->field[y+1]>>5) : 0ul); // migishita
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
    s->field[y] &= ~mask_vanish_y0;
    // count vanished block
    vcount += __builtin_popcountll(mask_vanish_y0 &
        ((0x21ul<<40) | (0x21ul<<30) | (0x21ul<<20) | (0x21ul<<10) | 0x21ul));
    // accumulate vanished block
    mask_vanish_y0_acc = mask_vanish_y1;
  }
  assert(!anyvanish || vcount>0);
  return vcount;
}

int drop(int offset, int rotnum, player_state_t *s) {
  pack_t pack = rotate(packs[s->turn_num], rotnum);
  fromto_t ft = NOFALL;
  // drop left half
  {
    int x = 8-offset+1;
    int y = s->top[x];
    if(pack.b[3]) {
      s->field[y+1] |= (uint64_t)pack.b[0] << 5*x;
      s->field[y+0] |= (uint64_t)pack.b[3] << 5*x;
      ft.from = y;
      ft.to   = y+1;
      s->top[x]+=2;
    } else {
      assert(pack.b[0]);
      s->field[y+0] |= (uint64_t)pack.b[0] << 5*x;
      ft.from = y;
      ft.to   = y;
      s->top[x]+=1;
    }
  }
  // drop right half
  {
    int x = 8-offset;
    int y = s->top[x];
    if(pack.b[2]) {
      s->field[y+1] |= (uint64_t)pack.b[1] << 5*x;
      s->field[y+0] |= (uint64_t)pack.b[2] << 5*x;
      ft.from = MIN(y  , ft.from);
      ft.to   = MAX(y+1, ft.to);
      s->top[x]+=2;
    } else {
      assert(pack.b[1]);
      s->field[y+0] |= (uint64_t)pack.b[1] << 5*x;
      ft.from = MIN(y  , ft.from);
      ft.to   = MAX(y  , ft.to);
      s->top[x]+=1;
    }
  }
  assert(!is_nofall(&ft)); // assert(ft != NOFALL)

  int chain = 0;
  while(vanish(s, ft)) {
    ft = fall(s);
    chain++;
  }
  return chain;
}

#define EVAL5_MAXY 13
int static_eval(player_state_t *s, int tail_col) {
  int score = 0;

  // count spaces and blocks around "5" excluding ojama
  uint16_t flag[EVAL5_MAXY] = {0};
  uint16_t ojama[EVAL5_MAXY] = {0};
  uint16_t zero[EVAL5_MAXY] = {0};
  for (int y = 0; y < EVAL5_MAXY; y++) {
    uint64_t row = s->field[y];
    for (int x = 0; x < 10; x++) {
      int val = row & 0x1F;
      row >>= 5;
      if(val==0)  zero[y]   |= 1<<x;
      if(val==11) ojama[y]  |= 1<<x;
      if(val!=5) continue;
      if(x>0) {flag[y-1] |= 7<<(x-1);}
              {flag[y  ] |= 7<<(x-1);}
      if(x<EVAL5_MAXY){flag[y+1] |= 7<<(x-1);}
    }
  }
  int around5=0, effective5=0;
  const uint16_t  range = (1<<10)-1;
  for (int y = 0; y < EVAL5_MAXY; y++) {
    around5     += __builtin_popcount(range & flag[y] & ~ojama[y]);
    effective5  += __builtin_popcount(range & flag[y] & ~ojama[y] & ~zero[y]);
  }
  score += skill_ojama[around5]*128;
  score += skill_ojama[effective5]*256;

  // evaluate additional chain
  for (int x = MAX(0, tail_col-1); x < MIN(9, tail_col+1); x++) {
    for (int num = 0; num < 10; num++) {
      //TODO
    }
  }

  return score;
}

int drop_and_eval(player_state_t *s, int offset, int rotnum) {
  int score = 0;
  // drop
  int chain = drop(offset, rotnum, s);
  score +=
    chain==0  ? 0 :
    chain<8   ? (chain_ojama[chain]-9)*256 :
                 chain_ojama[chain]   *1024;

  // fatal
  if(isfatal(s))    return INT_MIN/4;
  // danger
  if(s->field[15])  score -= 1024*1024*4;
  if(s->field[14])  score -= 1024*512;
  if(s->field[13])  score -= 1024*256;
  if(s->field[12])  score -= 1024*128;
  if(s->field[11])  score -= 1024*32;

  return score;
}

int search_comparator_dec(const void *a, const void *b) {
  return ((search_t*)b)->score - ((search_t*)a)->score;
}

search_t search(const player_state_t *s, int recurse_limit) {
  player_state_t  states[9*4];
  search_t        ops[9*4];
  int idx = 0;  // == rotnum*9 + offset
  for (int rotnum = 0; rotnum < 4; rotnum++)
  for (int offset = 0; offset < 9; offset++) {
    states[idx] = *s;   // copy field
    int score = drop_and_eval(&states[idx], offset, rotnum);
    score += static_eval(&states[idx], 0/*FIXME: dummy*/);
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
    const int max_search_op = 4;
    for (int i = 0; i < max_search_op; i++) {
      search_t *op = &ops[i];
      idx = op->rotnum*9 + op->offset;
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
      search_t best_op = search(&me, 7);
      dump_field(&me);
      printf("%d %d\n", best_op.offset, best_op.rotnum);
    }
    fflush(stdout);
  }

  return 0;
}
