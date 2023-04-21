all: remote

local: WirelessConnectionManager.cpp wifiobj.cpp
	g++ $^ `pkg-config --cflags --libs glib-2.0 libnm` -o $@
	
remote: WirelessConnectionManager.cpp wifiobj.cpp
	clean
	git pull
	g++ $^ `pkg-config --cflags --libs glib-2.0 libnm` -o $@

clean:
	rm -f wifilib
