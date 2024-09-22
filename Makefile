CC := g++
CFLAGS := -D _DEBUG -g -ggdb3 -std=c++17 -O0 -Wall -Wextra -Wno-missing-field-initializers -fsanitize=address,undefined,leak -I ./
LFLAGS := -fsanitize=address,undefined,leak
OBJDIR := obj

OBJ_FUNC = $(addprefix $(OBJDIR)/, $(1:.cpp=.o))
DEP_FUNC = $(addprefix $(OBJDIR)/, $(1:.cpp=.d))

RUN_FILE = run/main

SRCFILES := $(wildcard *.cpp)

.PHONY: all clean run

all: $(RUN_FILE)

clean:
	rm -r $(OBJDIR)
	rm    $(RUN_FILE)

run: $(RUN_FILE)
	cd $(dir $(RUN_FILE)) && ./$(notdir $(RUN_FILE))

OBJECTS := $(call OBJ_FUNC, $(SRCFILES))

$(RUN_FILE): $(OBJECTS)
	$(CC) $(LIB_OBJ) $(OBJECTS) -o $@ $(LFLAGS)


$(OBJDIR)/%.d : %.cpp
	mkdir -p $(dir $@)
	$(CC) $< -c -MMD -MP -o $(@:.d=.o) $(CFLAGS)

$(OBJDIR)/%.o : %.cpp
	mkdir -p $(dir $@)
	$(CC) $< -c -MMD -MP -o $@ $(CFLAGS)

include $(wildcard $(OBJDIR)/*.d)
