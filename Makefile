TARGET		= /tmp/aaikiso
CCFLAGS		= -lm -std=c11 -O3 -mtune=native -march=native -mfpmath=both -Wall -Wextra
SRCS			= ai.c ai.h
CC				= gcc

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $^ $(CCFLAGS) -o $@

prof: $(SRCS)
	$(CC) $^ $(CCFLAGS) -DTURN_END=20 -pg -o $(TARGET)

# solve dependencies
$(foreach SRC,$(SRCS),$(eval $(subst \,,$(shell $(CC) -MM $(SRC)))))
