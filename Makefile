IDF_EXPORT := . /Users/hyeonjun/Workspace/WeActStudio.ESP32S3-AorB/esp/esp-idf/export.sh >/dev/null 2>&1
PORT       ?= /dev/cu.usbmodem1101

.PHONY: preprocess-math build flash monitor flash-monitor clean

preprocess-math:
	npm run preprocess-math

build:
	$(MAKE) preprocess-math
	$(IDF_EXPORT) && idf.py build

flash:
	$(IDF_EXPORT) && idf.py -p $(PORT) flash

monitor:
	$(IDF_EXPORT) && idf.py -p $(PORT) monitor

flash-monitor:
	$(IDF_EXPORT) && idf.py -p $(PORT) flash monitor

clean:
	$(IDF_EXPORT) && idf.py fullclean
