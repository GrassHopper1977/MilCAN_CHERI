commonfiles := milcan.c milcan.h interfaces.c interfaces.h utils/priorities.c utils/priorities.h utils/timestamp.c utils/timestamp.h utils/logs.h ../BSD-USB-to-CAN/usb2can.h

all: milcan milcan_hy

milcan: $(commonfiles)
	cc -g -O2 -Wall -mabi=purecap -cheri-bounds=subobject-safe -lusb -lssl -o milcan milcan.c interfaces.c utils/timestamp.c utils/priorities.c

milcan_hy: $(commonfiles)
	cc -g -O2 -Wall -mabi=aapcs -cheri-bounds=subobject-safe -lusb -lssl -o milcan_hy milcan.c interfaces.c utils/timestamp.c utils/priorities.c

.PHONY: clean

clean:
	rm -f milcan milcan_hy
