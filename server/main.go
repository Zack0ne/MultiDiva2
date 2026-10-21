// MultiDiva server: rooms, chat, host song pick and live scores for the MultiDiva game mod.
// Protocol: docs/protocol.md.
package main

import (
	"flag"
	"log"
	"net/http"
	"time"
)

func main() {
	addr := flag.String("addr", ":3939", "listen address")
	tick := flag.Duration("tick", 100*time.Millisecond, "live score broadcast interval")
	loadTimeout := flag.Duration("load-timeout", 30*time.Second, "max wait on the loading screen for slow players")
	flag.Parse()

	log.Printf("MultiDiva server (protocol v%d) listening on %s", ProtocolVersion, *addr)
	log.Fatal(http.ListenAndServe(*addr, httpHandler(NewHub(*tick, *loadTimeout))))
}

func httpHandler(hub *Hub) http.Handler {
	mux := http.NewServeMux()
	mux.HandleFunc("/ws", hub.ServeWS)
	mux.HandleFunc("/avatar/", hub.ServeAvatar)
	mux.HandleFunc("/healthz", func(w http.ResponseWriter, _ *http.Request) { w.Write([]byte("ok")) })
	return mux
}
