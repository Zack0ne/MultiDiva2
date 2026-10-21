package main

import (
	"sort"
	"strings"
	"sync"
	"time"
)

// The room list for players who are connected but not in a room yet ("rooms" message). Any room change marks the
// list dirty; one update goes out a moment later, so a burst of changes (a round starting for 8 players) is a
// single message.
//
// Locking: the lobby has its own lock and never holds it while taking a room's or the hub's, so it can be called
// from under either (rooms call roomsChanged with their lock held).

const roomsDelay = 150 * time.Millisecond

type lobby struct {
	mu      sync.Mutex
	clients map[*Client]struct{}
	pending bool
}

func (h *Hub) enterLobby(c *Client) {
	h.lobby.mu.Lock()
	h.lobby.clients[c] = struct{}{}
	h.lobby.mu.Unlock()
	h.roomsChanged() // the newcomer gets the current list
}

func (h *Hub) leaveLobby(c *Client) {
	h.lobby.mu.Lock()
	delete(h.lobby.clients, c)
	h.lobby.mu.Unlock()
}

// roomsChanged schedules a room list update for everyone in the lobby.
func (h *Hub) roomsChanged() {
	h.lobby.mu.Lock()
	defer h.lobby.mu.Unlock()
	if h.lobby.pending {
		return
	}
	h.lobby.pending = true
	time.AfterFunc(roomsDelay, h.sendRooms)
}

func (h *Hub) sendRooms() {
	h.lobby.mu.Lock()
	h.lobby.pending = false
	clients := make([]*Client, 0, len(h.lobby.clients))
	for c := range h.lobby.clients {
		clients = append(clients, c)
	}
	h.lobby.mu.Unlock()
	if len(clients) == 0 {
		return
	}
	msg := encode("rooms", h.roomList())
	for _, c := range clients {
		c.Send(msg)
	}
}

func (h *Hub) roomList() RoomsMsg {
	h.mu.Lock()
	rooms := make([]*Room, 0, len(h.rooms))
	for _, r := range h.rooms {
		rooms = append(rooms, r)
	}
	h.mu.Unlock()
	list := RoomsMsg{Rooms: []RoomInfo{}}
	for _, r := range rooms {
		if info, ok := r.Summary(); ok {
			list.Rooms = append(list.Rooms, info)
		}
	}
	sort.Slice(list.Rooms, func(i, j int) bool { return strings.ToLower(list.Rooms[i].Name) < strings.ToLower(list.Rooms[j].Name) })
	return list
}

// Summary describes the room for the room list (false if it is closed or empty).
func (r *Room) Summary() (RoomInfo, bool) {
	r.mu.Lock()
	defer r.mu.Unlock()
	if r.closed || len(r.members) == 0 {
		return RoomInfo{}, false
	}
	info := RoomInfo{Name: r.name, Players: len(r.members), Playing: r.state == StatePlaying}
	if host := r.findID(r.host); host != nil {
		info.Host = host.c.name
	}
	return info, true
}
