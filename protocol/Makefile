PROTOC = protoc
OUT_DIR = ./proto_res

PROTOC_FLAGS = --cpp_out=${OUT_DIR}


PROTO_FILES := $(wildcard *.proto)
PROTO_CPP_FILES := $(patsubst %.proto,$(OUT_DIR)/%.pb.cc,$(PROTO_FILES))
PROTO_HDR_FILES := $(patsubst %.proto,$(OUT_DIR)/%.pb.h,$(PROTO_FILES))

all: $(PROTO_CPP_FILES) $(PROTO_HDR_FILES)

$(OUT_DIR)/%.pb.cc $(OUT_DIR)/%.pb.h: %.proto
	@mkdir -p $(OUT_DIR)
	$(PROTOC) $(PROTOC_FLAGS) $<

clean:
	rm -rf $(OUT_DIR)

.PHONY: all clean
