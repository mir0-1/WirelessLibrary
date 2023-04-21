all: wifilib

wifilib: WirelessConnectionManager.cpp wifiobj.cpp
	git pull
	g++ $^ `pkg-config --cflags --libs glib-2.0 libnm` -o $@

clean:
	rm -f wifilib
