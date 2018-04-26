NAME := cfrac

$(NAME)_CFLAGS += -Wall

$(NAME)_SOURCES := cfrac.c pops.c pconst.c pio.c \
          pabs.c pneg.c pcmp.c podd.c phalf.c \
          padd.c psub.c pmul.c pdivmod.c psqrt.c ppowmod.c \
          atop.c ptoa.c itop.c utop.c ptou.c errorp.c \
          pfloat.c pidiv.c pimod.c picmp.c primes.c pcfrac.c pgcd.c

#$(NAME)_DEFINES += IGNOREFREE
