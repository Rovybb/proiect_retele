all: build

build: server client

server: src/server/server.c src/server/handlers/serverHandlers.c
	gcc -o build/server src/server/server.c src/server/handlers/serverHandlers.c src/server/game/gameHandlers.c -lsqlite3 -lpthread

client: src/client/client.c src/client/handlers/clientHandlers.c
	gcc -o build/client src/client/client.c src/client/handlers/clientHandlers.c

clean:
	rm -f build/server build/client
