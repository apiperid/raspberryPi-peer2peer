# ============================================
# COMMANDS

CC = gcc -O3
RM = rm -f

# ==========================================
# TARGETS

EXECUTABLES = peer2peer onlyClient


all: $(EXECUTABLES)


peer2peer: peer2peer.c
	$(CC) $< -o $@ -lpthread

onlyClient: onlyClient.c
	$(CC) $< -o $@

clean:
	$(RM) *.o *~ $(EXECUTABLES)
