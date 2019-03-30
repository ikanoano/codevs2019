TARGET=/tmp/aaikiso
CCFLAGS=-lm -std=c11 -march=native -O2
SRCS=ai.c

all: /tmp/aaikiso

$(TARGET): $(SRCS)
	gcc $^ $(CCFLAGS) -o $@

prof: $(SRCS)
	gcc $^ $(CCFLAGS) -DTURN_END=20 -pg -o $(TARGET)
