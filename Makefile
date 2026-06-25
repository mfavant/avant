PROTO_DIR := ./protocol
LUA_PROTO_DIR := ./bin/lua/ProtoLua

.PHONY: proto clean

proto:
	node ./generate_proto_lua.js $(PROTO_DIR) $(LUA_PROTO_DIR)

clean:
	rm -rf $(LUA_PROTO_DIR)/*.lua
