parsing: parsing.c
	$(CC) parsing.c mpc.c -o parsing -Wall -Wextra -pedantic -std=c99 -ledit -ggdb
