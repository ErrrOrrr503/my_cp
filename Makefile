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
	mkdir $(TDIR)/dir-ro
	chmod a-w $(TDIR)/dir-ro
	mkdir $(TDIR)/dir-wo
	chmod a-r $(TDIR)/dir-wo
	ln -s user-rw $(TDIR)/user-rw-user-symlink
	ln -s nonuser-rw $(TDIR)/nonuser-rw-user-symlink
	sudo -u $(NONUSER) ln -s nonuser-rw $(TDIR)/nonuser-rw-nonuser-symlink
	#### testing file to file copy ####
	$(TDIR)/my_cp $(TDIR)/user-rw $(TDIR)/user-rw_c
	$(TDIR)/my_cp $(TDIR)/user-ro $(TDIR)/user-ro_c
	$(TDIR)/my_cp $(TDIR)/user-wo $(TDIR)/user-wo_c
	sudo $(TDIR)/my_cp -u $(TDIR)/nonuser-rw $(TDIR)/nonuser-rw_c
	#### testing file to dir copy  ####
	$(TDIR)/my_cp $(TDIR)/user-rw $(TDIR)/dir-rw/
	$(TDIR)/my_cp $(TDIR)/user-rw $(TDIR)/dir-ro
	$(TDIR)/my_cp $(TDIR)/user-rw $(TDIR)/dir-wo/
	#### testing symlink copying   #### 
	$(TDIR)/my_cp $(TDIR)/user-rw-user-symlink $(TDIR)/user-rw-user-symlink-c
	$(TDIR)/my_cp $(TDIR)/user-rw-user-symlink $(TDIR)/dir-rw/
	$(TDIR)/my_cp $(TDIR)/nonuser-rw-user-symlink $(TDIR)/dir-rw
	sudo $(TDIR)/my_cp -u $(TDIR)/nonuser-rw-nonuser-symlink $(TDIR)/dir-rw/
clean:
	rm my_cp
clean_tests:
	rm -rf tests
