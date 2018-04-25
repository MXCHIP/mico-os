NAME := entry

$(NAME)_TYPE := kernel

GLOBAL_INCLUDES += .

$(NAME)_CFLAGS += -marm

$(NAME)_INCLUDES := ../../app      \
				   ../../app/config \
                   ../../func/include \
                   ../../os/include \
                   ../../ip/lmac/src/rwnx \
                   ../../ip/ke \
                   ../../ip/mac \
                   ../../../../aos 
					
                   
$(NAME)_SOURCES	 += arch_main.c \
                    boot_handlers.S \
                    boot_vectors.S \
                    ll.S \
                    ../intc/intc.c 
