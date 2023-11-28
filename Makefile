CFLAGS = -g -O2 -Wall -cheri-bounds=subobject-safe
LFLAGS = -lGSUSB
LIBFLAGS= -fPIC -shared
PURECAP = -mabi=purecap
HYBRID = -mabi=aapcs

HEADERFILES = milcan.h interfaces.h CANdoC.h can.h gsusb.h txq.h utils/timestamp.h utils/priorities.h utils/logs.h
COMMONSOURCEFILES = utils/timestamp.c utils/priorities.c
LIBSOURCEFILES = milcan.c interfaces.c CANdoC.c txq.c $(COMMONSOURCEFILES)
APPSOURCEFILES = test.c $(COMMONSOURCEFILES)
APP2SOURCEFILES = test2.c $(COMMONSOURCEFILES)
ALLFILES= $(LIBSOURCEFILES) $(HEADERFILES) $(APPSOURCEFILES) $(APP2SOURCEFILES)

all: libMILCAN.so libMILCAN_hy.so test test_hy test2 test2_hy 

libMILCAN.so: $(ALLFILES)
	cc $(PURECAP) $(CFLAGS) $(LIBFLAGS) $(LIBSOURCEFILES) $(LFLAGS) -olibMILCAN.so
	cp libMILCAN.so /usr/lib

libMILCAN_hy.so: $(LIBSOURCEFILES) $(HEADERFILES)
	cc $(HYBRID) $(CFLAGS) $(LIBFLAGS) $(LIBSOURCEFILES) $(LFLAGS) -olibMILCAN_hy.so
	cp libMILCAN_hy.so /usr/lib64/libMILCAN.so

test: $(ALLFILES)
	cc $(PURECAP) $(CFLAGS) -lMILCAN $(APPSOURCEFILES) -o test

test_hy: $(ALLFILES)
	cc $(HYBRID) $(CFLAGS) -lMILCAN $(APPSOURCEFILES) -o test_hy 

test2: $(ALLFILES)
	cc $(PURECAP) $(CFLAGS) -lMILCAN $(APP2SOURCEFILES) -o test2

test2_hy: $(ALLFILES)
	cc $(HYBRID) $(CFLAGS) -lMILCAN $(APP2SOURCEFILES) -o test2_hy 

.PHONY: clean

clean:
	rm -f milcan milcan_hy test test_hy test2 test2_hy milcan.so milcan_hy.so
