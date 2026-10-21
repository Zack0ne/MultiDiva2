package main

import (
	"context"
	"log"
	"net/http"
	"strings"
	"sync"
	"time"

	"github.com/coder/websocket"
)

// Hub owns the rooms and hands out player ids.
type Hub struct {
	avatars     map[int]Avatar
	mu          sync.Mutex
	rooms       map[string]*Room
	nextID      int
	tickEvery   time.Duration
	loadTimeout time.Duration
	lobby       lobby // connected players not in a room, who get the room list
}

func NewHub(tickEvery, loadTimeout time.Duration) *Hub {
	return &Hub{rooms: map[string]*Room{}, avatars: map[int]Avatar{}, tickEvery: tickEvery, loadTimeout: loadTimeout,
		lobby: lobby{clients: map[*Client]struct{}{}}}
}

// Room returns the room with this name (case-insensitive), creating it if needed.
func (h *Hub) Room(name string) *Room {
	h.mu.Lock()
	defer h.mu.Unlock()
	key := strings.ToLower(name)
	r, ok := h.rooms[key]
	if !ok {
		r = newRoom(h, key, name)
		h.rooms[key] = r
		log.Printf("room %q created", name)
	}
	return r
}

// removeIfEmpty is called by a room after its last player left.
func (h *Hub) removeIfEmpty(r *Room) {
	h.mu.Lock()
	defer h.mu.Unlock()
	if h.rooms[r.key] == r && r.Empty() {
		delete(h.rooms, r.key)
		r.Close()
		log.Printf("room %q closed", r.name)
		h.roomsChanged()
	}
}

func (h *Hub) newID() int {
	h.mu.Lock()
	defer h.mu.Unlock()
	h.nextID++
	return h.nextID
}

func (h *Hub) ServeWS(w http.ResponseWriter, r *http.Request) {
	conn, err := websocket.Accept(w, r, &websocket.AcceptOptions{
		// The game client isn't a browser; there is no origin to check.
		InsecureSkipVerify: true,
	})
	if err != nil {
		return
	}

	ctx, cancel := context.WithCancel(r.Context())
	defer cancel()

	c := &Client{id: h.newID(), hub: h, conn: conn, send: make(chan []byte, sendBuffer)}
	log.Printf("client %d connected from %s", c.id, r.RemoteAddr)

	go c.writeLoop(ctx)
	c.readLoop(ctx)

	h.setAvatar(c.id, nil)
	h.leaveLobby(c)
	if c.room != nil {
		c.room.Leave(c)
	}
	conn.Close(websocket.StatusNormalClosure, "")
	log.Printf("client %d (%s) disconnected", c.id, c.name)
}
