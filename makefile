all: remote

OUTPUT_NAME=wifilib
COMPILER=g++
CONFIG=`pkg-config --cflags --libs glib-2.0 libnm`
BUILD=$(COMPILER) $^ $(CONFIG) -o $(OUTPUT_NAME)

local: WirelessConnectionManager.cpp ../EventManger/EventManager.cpp wifiobj.cpp
	make clean
	$(BUILD)
	
remote: WirelessConnectionManager.cpp wifiobj.cpp
	make -C ../EventManger
	make clean
	git pull
	$(BUILD)

clean:
	rm -f $(OUTPUT_NAME)
