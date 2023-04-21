all: remote

COMPILER=g++
CONFIG=`pkg-config --cflags --libs glib-2.0 libnm`
BUILD=$(COMPILER) $^ $(CONFIG) -o $@

local: WirelessConnectionManager.cpp wifiobj.cpp
	make clean
	$(BUILD)
	
remote: WirelessConnectionManager.cpp wifiobj.cpp
	make clean
	git pull
	$(BUILD)

clean:
	rm -f wifilib
