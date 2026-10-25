# GNU Makefile for Linux & macOS.

MFILE_OS=Makefile

# Note: xu4 is hardcoded to look in /usr & /usr/local only!
DESTDIR ?= /usr/local

MODULES=render.pak Ultima-IV.mod U4-Upgrade.mod Apple-II.mod Apple-II-Music.mod \
	C64.mod C64-Music.mod C64-Remastered.mod SMS.mod SMS-Music.mod
BORON=boron
REND=module/render


all: src/xu4 $(MODULES)

src/xu4:
	make -C src -f $(MFILE_OS) xu4

render.pak: $(REND)/shader/*.glsl $(REND)/shader/*.png $(REND)/font/cfont.png \
	$(REND)/font/*.txf
	$(BORON) -s tools/pack-xu4.b -f $(REND) -o $@

Ultima-IV.mod: module/Ultima-IV/*.b
	$(BORON) -s tools/pack-xu4.b module/Ultima-IV

U4-Upgrade.mod: module/U4-Upgrade/*.b
	$(BORON) -s tools/pack-xu4.b module/U4-Upgrade

Apple-II.mod: module/Apple-II/*.b
	$(BORON) -s tools/pack-xu4.b module/Apple-II

Apple-II-Music.mod: module/Apple-II-Music/*.b
	$(BORON) -s tools/pack-xu4.b module/Apple-II-Music

C64.mod: module/C64/*.b
	$(BORON) -s tools/pack-xu4.b module/C64

C64-Music.mod: module/C64-Music/*.b
	$(BORON) -s tools/pack-xu4.b module/C64-Music

C64-Remastered.mod: module/C64-Remastered/*.b
	$(BORON) -s tools/pack-xu4.b module/C64-Remastered

SMS.mod: module/SMS/*.b
	$(BORON) -s tools/pack-xu4.b module/SMS

SMS-Music.mod: module/SMS-Music/*.b
	$(BORON) -s tools/pack-xu4.b module/SMS-Music

.PHONY: clean download mod

clean:
	make -C src -f $(MFILE_OS) clean
	rm -f $(MODULES)

mod: $(MODULES)

download: ultima4.zip u4upgrad.zip

ultima4.zip:
	curl -sSL -o $@ http://ultima.thatfleminggent.com/ultima4.zip

u4upgrad.zip:
	curl -sSL -o $@ http://sourceforge.net/projects/xu4/files/Ultima%204%20VGA%20Upgrade/1.3/u4upgrad.zip

install: src/xu4 $(MODULES) u4upgrad.zip
	mkdir -p $(DESTDIR)/bin $(DESTDIR)/share/xu4
	install -m 755 -s src/xu4 $(DESTDIR)/bin/xu4
	install -m 644 -t $(DESTDIR)/share/xu4 $(MODULES) u4upgrad.zip
	install -D -m 644 icons/shield.png $(DESTDIR)/share/icons/hicolor/48x48/apps/xu4.png
	install -D -m 644 dist/xu4.desktop $(DESTDIR)/share/applications/xu4.desktop
