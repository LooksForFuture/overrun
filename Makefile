BIN_DIR = bin

default: build-run

ready:
	mkdir -p $(BIN_DIR)/
	cd $(BIN_DIR)/ &&\
	cmake .. -G Ninja

build:
	cd $(BIN_DIR)/ &&\
	cmake --build .

run:
	cd $(BIN_DIR)/ && \
	./game

build-run:
	cd $(BIN_DIR)/ && \
	cmake --build . && \
	./game

test-ryu:
	cd $(BIN_DIR)/engine/ryu/ && \
	ctest --output-on-failure

.PHONY: ready build run build-run test-ryu
