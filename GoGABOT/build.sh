g++ -g -std=c++17 -Ofast -pthread -lncurses -o GoGABOT enet/callbacks.c enet/compress.c enet/host.c enet/list.c enet/packet.c enet/peer.c enet/protocol.c enet/unix.c enet/win32.c events.cpp gt.cpp world.cpp  main.cpp sandbird/sandbird.c server.cpp utils.cpp