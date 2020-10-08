CC = gcc
GDB = gdb
CFLAGS += -Wall -Wextra -Og
TDIR = ./tests
NONUSER = root
all:
	$(CC) $(CFLAGS) -o my_cp main.c
	chmod +x my_cp
tests:
	mkdir $(TDIR)
	cp my_cp $(TDIR)/my_cp
	dd if=/dev/urandom of=$(TDIR)/user-rw bs=1M count=1
	dd if=/dev/urandom of=$(TDIR)/user-ro bs=1M count=1
	chmod a-w-x $(TDIR)/user-ro
	dd if=/dev/urandom of=$(TDIR)/user-wo bs=1M count=1
	chmod a-r-x $(TDIR)/user-wo
	dd if=/dev/urandom of=$(TDIR)/nonuser-rw bs=1M count=1
	sudo chown $(NONUSER) $(TDIR)/nonuser-rw
	mkdir $(TDIR)/dir-rw
	mkdir $(TDIR)/dir-rw/dir-1
	mkdir $(TDIR)/dir-rw/dir-1/dir-2
	mkdir $(TDIR)/dir-rw/dir-2
	mkdir $(TDIR)/dir-ro
	chmod a-w $(TDIR)/dir-ro
	mkdir $(TDIR)/dir-wo
	chmod a-r $(TDIR)/dir-wo
	ln -s user-rw $(TDIR)/user-rw-user-symlink
	ln -s nonuser-rw $(TDIR)/nonuser-rw-user-symlink
	sudo -u $(NONUSER) ln -s nonuser-rw $(TDIR)/nonuser-rw-nonuser-symlink
	#### testing file to file copy ####
	$(TDIR)/my_cp $(TDIR)/user-rw $(TDIR)/user-rw-c
	$(TDIR)/my_cp $(TDIR)/user-ro $(TDIR)/user-ro-c
	$(TDIR)/my_cp $(TDIR)/user-wo $(TDIR)/user-wo-c
	$(TDIR)/my_cp -t $(TDIR)/user-rw $(TDIR)/user-rw-c-t
	sudo $(TDIR)/my_cp -u $(TDIR)/nonuser-rw $(TDIR)/nonuser-rw-c
	#### testing file to dir copy  ####
	$(TDIR)/my_cp $(TDIR)/user-rw $(TDIR)/dir-rw/
	$(TDIR)/my_cp $(TDIR)/user-rw $(TDIR)/dir-ro
	$(TDIR)/my_cp $(TDIR)/user-rw $(TDIR)/dir-wo/
	$(TDIR)/my_cp $(TDIR)/user-rw $(TDIR)/dir-wo/dir-1
	$(TDIR)/my_cp $(TDIR)/user-rw $(TDIR)/dir-wo/dir-1/dir-2
	$(TDIR)/my_cp -utf $(TDIR)/nonuser-rw $(TDIR)/dir-rw/
	$(TDIR)/my_cp -utf $(TDIR)/nonuser-rw $(TDIR)/dir-rw/dir-1
	$(TDIR)/my_cp -utf $(TDIR)/nonuser-rw $(TDIR)/dir-rw/dir-2
	$(TDIR)/my_cp -utf $(TDIR)/nonuser-rw $(TDIR)/dir-rw/dir-1/dir-2
	#### testing symlink copying   #### 
	$(TDIR)/my_cp $(TDIR)/user-rw-user-symlink $(TDIR)/user-rw-user-symlink-c
	$(TDIR)/my_cp $(TDIR)/user-rw-user-symlink $(TDIR)/dir-rw/
	$(TDIR)/my_cp $(TDIR)/user-rw-user-symlink $(TDIR)/dir-rw/dir-2
	$(TDIR)/my_cp $(TDIR)/user-rw-user-symlink $(TDIR)/dir-rw/dir-1/dir-2
	$(TDIR)/my_cp -utf $(TDIR)/nonuser-rw-user-symlink $(TDIR)/dir-rw
	sudo $(TDIR)/my_cp -u $(TDIR)/nonuser-rw-nonuser-symlink $(TDIR)/dir-rw/
	$(TDIR)/my_cp -t $(TDIR)/user-rw-user-symlink $(TDIR)/user-rw-user-symlink-c-t
	#### testind dir to dir copying ####
	mkdir $(TDIR)/dir-rw-c/
	sudo $(TDIR)/my_cp -uft $(TDIR)/dir-rw $(TDIR)/dir-rw-c/
	#### testing xattr copying ####
	$(TDIR)/my_cp $(TDIR)/user-rw $(TDIR)/user-rw-xattr
	setfattr -n user.fred -v chocolate $(TDIR)/user-rw-xattr
	setfattr -n user.frieda -v bar $(TDIR)/user-rw-xattr
	setfattr -n user.empty $(TDIR)/user-rw-xattr
	$(TDIR)/my_cp $(TDIR)/user-rw-xattr $(TDIR)/user-rw-xattr-no
	$(TDIR)/my_cp -x $(TDIR)/user-rw-xattr $(TDIR)/user-rw-xattr-yes
clean:
	rm my_cp
clean_tests:
	rm -rf tests
