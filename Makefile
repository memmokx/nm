NAME = ft_nm
CC ?= cc

CFLAGS = -std=c23 -Wall -Wextra -Werror -Wno-unknown-warning-option -Wno-error=old-style-declaration
LIBAD = libadvanced/libad.a
INCLUDE = -Iinclude -Ilibadvanced/include

MAIN_SRC = src/main.c src/elfu.c src/sort.c src/opt.c

SRC = $(MAIN_SRC)
OBJ = $(SRC:.c=.o)

COLOUR_GREEN=$(shell tput setaf 2)
COLOUR_GRAY=$(shell tput setaf 254)
COLOUR_RED=$(shell tput setaf 1)
COLOUR_BLUE=$(shell tput setaf 4)
BOLD=$(shell tput bold)
COLOUR_END=$(shell tput sgr0)

ifdef OPT
	CFLAGS += -march=native -O3 -flto
endif

ifdef DEBUG
	CFLAGS += -g2
endif

ifdef SANITIZE
	CFLAGS += -g -fsanitize=address,undefined,leak
endif

ifndef NO_SILENT
.SILENT:
endif

all: $(NAME)

$(NAME): $(OBJ) $(LIBAD)
	$(CC) $(CFLAGS) $^ -o $@ $(INCLUDE)
	@echo "$(COLOUR_GREEN)Compiled:$(COLOUR_END) $(BOLD)$@$(COLOUR_END)"

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@ $(INCLUDE)
	@echo "$(COLOUR_BLUE)Compiled:$(COLOUR_END) $< $(COLOUR_GRAY)$(CC) $(CFLAGS)$(COLOUR_END)"

$(LIBAD):
	@$(MAKE) -C libadvanced -j

format:
	clang-format -i $(SRC) $(TEST_SRC)

clean:
	@rm -f $(OBJ)
	@$(MAKE) -C libadvanced clean

fclean: clean
	@rm -f $(NAME)
	@$(MAKE) -C libadvanced fclean

re : fclean all

.PHONY: re all fclean clean format
