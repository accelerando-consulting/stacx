build: $(OBJ)

$(OBJ): $(SRCS) Makefile
	arduino-cli compile -b $(BOARD) --build-cache-path . $(CCFLAGS) $(MAIN)

clean:
	rm -f $(OBJ)

ota: $(OBJ)
	@if [ -z "$$IP" ] ; then \
		IP=`avahi-browse -ptr  "_arduino._tcp" | egrep ^= | cut -d\; -f4,8,9 | grep ^$$DEVICE | cut -d\; -f2` -p `avahi-browse -ptr  "_arduino._tcp" | egrep ^= | cut -d\; -f4,8,9 | grep ^$$DEVICE | cut -d\; -f3` ;\
	fi ;\
	python $(OTAPROG) -i $(IP) "--auth=$(OTAPASS)" -f $(OBJ)

find:
	@if [ `uname -s` = Darwin ] ; then \
		dns-sd -B _arduino._tcp ;\
	else \
		avahi-browse -ptr  "_arduino._tcp" | egrep ^= | cut -d\; -f4,8,9 ;\
	fi

upload: #$(OBJ)
ifeq ($(PROXYHOST),)
	python $(ESPTOOL) --port $(PORT) write_flash 0x10000 $(OBJ)
#	arduino-cli upload -b $(BOARD) -p $(PORT) -i $(OBJ) -v -t
else
	scp $(OBJ) $(PROXYHOST):tmp/$(PROGRAM).ino.bin
	ssh -t $(PROXYHOST) $(ESPTOOL) -p $(PROXYPORT) write_flash 0x10000 tmp/$(PROGRAM).ino.bin
endif

erase:
ifeq ($(PROXYHOST),)
	python $(ESPTOOL) --port $(PORT) erase_flash
else
	ssh -t $(PROXYHOST) esptool.py --port $(PORT) erase_flash
endif

monitor sho:
ifeq ($(PROXYHOST),)
#	cu -s 115200 -l $(PORT)
	miniterm --rts 0 --dtr 0 $(PORT) 115200
else
	ssh -t $(PROXYHOST) miniterm --raw --rts 0 --dtr 0 $(PROXYPORT) 115200
endif

go: build upload

gosho: go monitor

installcli: 
	@[ -f `which arduino-cli` ] || go get -v -u github.com/arduino/arduino-cli && arduino-cli core update-index

installcore: cliconfig installcli
	@cat arduino-cli.yaml && arduino-cli core update-index && ls -l ~/.arduino15
	@arduino-cli core list
	@arduino-cli core list | grep ^esp8266:esp8266 >/dev/null || arduino-cli core install esp8266:esp8266
	@arduino-cli core list | grep ^esp32:esp32 >/dev/null || arduino-cli core install esp32:esp32

cliconfig:
	@ [ -d $(GOPATH) ] || mkdir -p $(GOPATH)
	@ [ -d $(GOPATH)/bin ] || mkdir -p $(GOPATH)/bin
	@ [ -d $(GOPATH)/src ] || mkdir -p $(GOPATH)/src
	@if [ \! -f $(GOPATH)/arduino-cli.yaml ] ; then \
	echo "board_manager:" >>$(GOPATH)/arduino-cli.yaml ; \
	echo "  additional_urls:" >>$(GOPATH)/arduino-cli.yaml ; \
	echo "    - http://arduino.esp8266.com/stable/package_esp8266com_index.json" >>$(GOPATH)/arduino-cli.yaml ; \
	echo "    - https://dl.espressif.com/dl/package_esp32_index.json" >>$(GOPATH)/arduino-cli.yaml ; \
	fi

libs:
	@for lib in $(LIBS) ; \
	do libdir=`echo "$$lib" | sed -e 's/ /_/g'` ; \
	  if [ -d "$(LIBDIR)/$$libdir" ] ; \
	  then \
	    true ; \
	  else \
	    echo "Installing $$lib" ; \
	    arduino-cli lib install "$$lib" ; \
          fi ;\
        done

extralibs:
	@[ -d $(LIBDIR) ] || mkdir -p $(LIBDIR)
	@for lib in $(EXTRALIBS) ; \
	do repo=`echo $$lib | cut -d@ -f2` ; \
	  dir=`echo $$lib | cut -d@ -f1`; \
	  if [ -d "$(LIBDIR)/$$dir" ] ; \
	  then \
	    echo "Found $$dir" ; \
	  else \
	    echo "Clone $$repo => $$dir" ; \
	    cd $(LIBDIR) && git clone $$repo $$dir ; \
          fi ; \
	done

installdeps: installcore libs extralibs
