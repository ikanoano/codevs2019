#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define NELEMS(x) ((int)(sizeof(x) / sizeof((x)[0])))
#define DEBUG(...) fprintf(stderr, __VA_ARGS__)

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
  for (int y = 17; y >= 0; y--) {
    for (int x = 9; x >= 0; x--) {
      fprintf(logger, "%2lx", ((field[y]>>(6*x)) & 0x3Ful));
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
    for (int j = 0; j < 10 ; j++) row |= (uint64_t)block[j] << (6*j);
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

int fall(uint64_t field[18]) {
  int anyfall = 0;
  for (int x = 0; x < 10; x++) {
    int top = 0;
    for (int y = 0; y < 18; y++) {
      uint64_t mask   = 0x3Ful << (6*x);
      uint64_t focus  = field[y] & mask;
      if(!focus) continue;

      if(top!=y) { // fall
        field[y  ] &= ~mask;
        assert((field[top] & mask) == 0); //field[top] &= ~mask;
        field[top] |= focus;
        anyfall = ~0;
      }
      top++;
    }
  }
  return anyfall;
}

int vanish(uint64_t field[18]) {
  uint64_t mask_vanish[18] = {0};
  int anyvanish = 0;

  // yoko vanish
  for (int y = 0; y < 18; y++) {
    uint64_t  wa_yoko = field[y] + (field[y]>>6);
    for (int x = 0; x<9; x++, wa_yoko>>=6) {
      if((wa_yoko & 0x1Ful) == 10) mask_vanish[y] |= 0xFFFul<<(6*x); // vanish
      anyvanish = 1;
    }
  }

  // tate & naname vanish
  for (int y = 0; y < 17; y++) {
    uint64_t  wa1 = (field[y]   ) + (field[y+1]   );
    uint64_t  wa2 = (field[y]>>6) + (field[y+1]   );
    uint64_t  wa3 = (field[y]   ) + (field[y+1]>>6);
    for (int x = 0; x < 10; x++, wa1>>=6, wa2>>=6, wa3>>=6) {
      if((wa1 & 0x1Ful) == 10) {
        // tate vanish
        mask_vanish[y+1] |= 0x3Ful<<(6*x);
        mask_vanish[y  ] |= 0x3Ful<<(6*x);
        anyvanish = 1;
      }
      if((wa2 & 0x1Ful) == 10) {
        // naname vanish 1
        mask_vanish[y+1] |= 0x3Ful<<(6* x   );
        mask_vanish[y  ] |= 0x3Ful<<(6*(x+1));
        anyvanish = 1;
      }
      if((wa3 & 0x1Ful) == 10) {
        // naname vanish 2
        mask_vanish[y+1] |= 0x3Ful<<(6*(x+1));
        mask_vanish[y  ] |= 0x3Ful<<(6* x   );
        anyvanish = 1;
      }
    }
  }
  if(!anyvanish) return 0;

  // execute
  for (int y = 0; y < 18; y++) {
    field[y] &= ~mask_vanish[y];
  }

  // count vanished block
  int vcount = 0;
  for (int y = 0; y < 18; y++) {
    for (int x = 0; x < 10; x++) {
      if(mask_vanish[y] & (0x1ul<<(6*x))) vcount++;
    }
  }
  assert(vcount>0);
  return vcount;
}

static inline pack_t rotate(pack_t pack, int rotnum) {
  pack_t rp;
  rp.raw = (pack.raw << (rotnum*8)) | (pack.raw >> ((4-rotnum)*8));
  return rp;
}
int drop(int offset, int rotnum, uint64_t field[18], pack_t pack) {
  pack = rotate(pack, rotnum);
  uint64_t mask = (0xFFFul << 6*(8-offset));
  assert((field[17] & mask) == 0);
  assert((field[16] & mask) == 0);
  field[17] |= (((uint64_t)pack.b[0]<<6) | pack.b[1]) << 6*(8-offset);
  field[16] |= (((uint64_t)pack.b[3]<<6) | pack.b[2]) << 6*(8-offset);
  int combo = -1;
  do {
    fall(field);
    combo++;
  } while(vanish(field));
  return combo;
}

int main(/*int argc, char const* argv[]*/) {
  init();
  printf("aaikiso\n");
  fflush(stdout);

  uint64_t po[18] = {0};
  po[17] = 0x040040040040040ul;
  po[16] = 0x005004003002001ul;
  po[ 1] = 0x040040040040040ul;
  po[ 0] = 0x005004003002001ul;
  dump_field(0,po);

  init_input();

  while(1) {
    player_state_t  me, rival;
    turn_input(&me, &rival);

    if(me.skill_charge>=80) {
      printf("S\n");
    } else {
      int offset  = rand()%9;
      int rotnum  = rand()%4;
      int combo = drop(offset, rotnum, me.field, packs[me.turn_num]);
      dump_field(me.turn_num, me.field);
      DEBUG("%2d combo\n", combo);
      printf("%d %d\n", offset, rotnum);
    }
    fflush(stdout);
  }

  return 0;
}
