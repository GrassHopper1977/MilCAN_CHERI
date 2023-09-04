commonfiles := milcan.c milcan.h timestamp.c timestamp.h logs.h ../BSD-USB-to-CAN/usb2can.h

all: milcan milcan_hy

milcan: $(commonfiles)
	cc -g -O2 -Wall -mabi=purecap -cheri-bounds=subobject-safe -lusb -lssl -o milcan milcan.c timestamp.c

milcan_hy: $(commonfiles)
	cc -g -O2 -Wall -mabi=aapcs -cheri-bounds=subobject-safe -lusb -lssl -o milcan_hy milcan.c timestamp.c

.PHONY: clean

clean:
	rm -f milcan milcan_hy
