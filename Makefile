CC = gcc
GDB = gdb
CFLAGS += -Wall -Wextra -Og
all:
	$(CC) $(CFLAGS) -o my_cp main.c
	chmod +x my_cp
tests_1:
	mkdir tests
	cp my_cp ./tests/my_cp
	dd if=/dev/urandom of=./tests/user-rw bs=1M count=1
	dd if=/dev/urandom of=./tests/user-ro bs=1M count=1
	chmod a-w-x ./tests/user-ro
	dd if=/dev/urandom of=./tests/user-wo bs=1M count=1
	chmod a-r-x ./tests/user-wo
	dd if=/dev/urandom of=./tests/nonuser-rw bs=1M count=1
	sudo chown root ./tests/nonuser-rw
	mkdir ./tests/dir-rw
	mkdir ./tests/dir-ro
	chmod a-w ./tests/dir-ro
	mkdir ./tests/dir-wo
	chmod a-r ./tests/dir-wo
	ln -s user-rw ./tests/user-rw-symlink
	./tests/my_cp ./tests/user-rw ./tests/user-rw_c
	./tests/my_cp ./tests/user-ro ./tests/user-ro_c
	./tests/my_cp ./tests/user-wo ./tests/user-wo_c
	./tests/my_cp ./tests/nonuser-rw ./tests/nonuser-rw_c
	./tests/my_cp ./tests/user-rw ./tests/dir-rw/
	./tests/my_cp ./tests/user-rw ./tests/dir-ro
	./tests/my_cp ./tests/user-rw ./tests/dir-wo/
	./tests/my_cp ./tests/user-rw-symlink ./tests/user-rw-symlink-c
	./tests/my_cp ./tests/user-rw-symlink ./tests/dir-rw/
clean:
	rm my_cp
clean_tests:
	rm -rf tests
