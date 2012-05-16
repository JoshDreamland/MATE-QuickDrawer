INCLUDES=`pkg-config --cflags libmatepanelapplet-3.0 libmate-menu gio-unix-2.0 glib-2.0`
LIBS=`pkg-config --libs libmatepanelapplet-3.0 libmate-menu glib-2.0`
FLAGS=-Wall -Werror -pedantic

default: bin/quickdrawer.app

bin:
	mkdir bin
objects:
	mkdir objects
objects/quickdrawer.o: quickdrawer.c quickdrawer.h preferences.h stored.h
	gcc -c quickdrawer.c -o objects/quickdrawer.o $(INCLUDES) $(FLAGS)
objects/highlight.o: highlight.c
	gcc -c highlight.c -o objects/highlight.o $(INCLUDES)  $(FLAGS)
objects/stored.o: stored.c stored.h quickdrawer.h
	gcc -c stored.c -o objects/stored.o $(INCLUDES) -std=c99 $(FLAGS)
objects/preferences.o: preferences.c preferences.h quickdrawer.h
	gcc -c preferences.c -o objects/preferences.o $(INCLUDES)  $(FLAGS)
bin/quickdrawer.app: bin objects objects/quickdrawer.o objects/highlight.o objects/stored.o objects/preferences.o
	gcc objects/*.o $(LIBS) -o bin/quickdrawer.app

INSTALL: install
Install: install
install: bin/quickdrawer.app
	cp res/org.mate.panel.applet.QuickdrawerAppletFactory.service /usr/share/dbus-1/services/
	cp res/org.mate.panel.Quickdrawer.mate-panel-applet /usr/share/mate-panel/applets/
	cp res/mate-quickdrawer.svg /usr/share/pixmaps/
	cp res/quickdrawer-notset.svg /usr/share/pixmaps/
	cp bin/quickdrawer.app /usr/lib/mate-panel/

clean:
	rm objects/*
	rm bin/*

