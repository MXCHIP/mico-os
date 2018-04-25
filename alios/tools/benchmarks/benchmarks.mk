NAME := benchmarks

$(NAME)_CFLAGS += -Wall

$(NAME)_SOURCES := benchmarks.c

$(NAME)_COMPONENTS  += benchmarks.cfrac

$GLOBAL_DEFINES += CONFIG_CMD_BENCHMARKS

#$(NAME)_DEFINES += IGNOREFREE
