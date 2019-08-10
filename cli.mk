installcli: 
	@[ -f $(GOPATH)/bin/arduino-cli ] || go get -v -u github.com/arduino/arduino-cli && arduino-cli core update-index

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

