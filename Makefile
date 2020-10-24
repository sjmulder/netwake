all:   win-all   unix-all
clean: win-clean unix-clean
dist:  win-dist  unix-all
.PHONY: all clean dist

win-all:   ; $(MAKE) -C win all
win-clean: ; $(MAKE) -C win clean
win-dist:  ; $(MAKE) -C win dist
.PHONY: win-all win-clean win-dist

unix-all:   ; $(MAKE) -C unix all
unix-clean: ; $(MAKE) -C unix clean
.PHONY: unix-all unix-clean

