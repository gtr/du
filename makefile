du:
	gcc du.c -o du

test1:
	./du

test2:
	./du ~/Documents

test3:
	./du ~/Documents -a

test4:
	./du -a ~/Documents

test5:
	./du -a

clean:
	rm du