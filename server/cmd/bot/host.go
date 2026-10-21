package main

import (
	"encoding/base64"
	"encoding/binary"
	"log"
	"time"
)

// Host mode: when a player gives the bot host, it runs a scripted match, so someone testing alone can
// check the guest side of a round:
//  1. hover a song nobody has, then start it (the player sees "no song" and sits out; the bot plays alone),
//  2. hover the last real song played in the room, then start it once the player is ready (the player's
//     game auto-starts it, with their own outfits),
//  3. give host back.

const (
	missingPV    = 9999 // no such song
	missingTitle = "Miku's Secret Song (test)"
	soloRound    = 15 * time.Second
)

type roomState struct {
	State   string `json:"state"`
	Host    int    `json:"host"`
	Players []struct {
		ID      int  `json:"id"`
		Ready   bool `json:"ready"`
		Missing bool `json:"missing"`
	} `json:"players"`
}

type hostScript struct {
	active     bool
	phase      int
	at         time.Time // next step not before this
	sawPlaying bool      // the round of the current phase has started
}

func chat(send func(string, any), text string) { send("chat", map[string]any{"text": text}) }

// withPV returns a copy of the pick for another song id (the launch block starts with the pv id).
func withPV(pick map[string]any, pv int, title string) map[string]any {
	out := map[string]any{}
	for k, v := range pick {
		out[k] = v
	}
	if block, err := base64.StdEncoding.DecodeString(pick["block"].(string)); err == nil && len(block) >= 4 {
		binary.LittleEndian.PutUint32(block, uint32(pv))
		out["block"] = base64.StdEncoding.EncodeToString(block)
	}
	out["pv"], out["title"] = pv, title
	return out
}

func (h *hostScript) step(send func(string, any), room roomState, myID int, lastStart map[string]any) {
	isHost := myID >= 0 && room.Host == myID
	if !isHost {
		*h = hostScript{} // given host again later: the script runs again
		return
	}
	now := time.Now()
	if !h.active {
		if room.State != "lobby" || len(room.Players) < 2 { // alone in a new room: nobody to test with
			return
		}
		*h = hostScript{active: true, at: now.Add(2 * time.Second)}
		return
	}
	if now.Before(h.at) {
		return
	}
	next := func(phase int, wait time.Duration) { h.phase, h.at, h.sawPlaying = phase, now.Add(wait), false }
	roundOver := func() bool { // the phase's round started and is back in the lobby
		if room.State == "playing" {
			h.sawPlaying = true
		}
		return h.sawPlaying && room.State == "lobby"
	}

	switch h.phase {
	case 0:
		if lastStart == nil {
			chat(send, "I'm the host! I don't know any song yet: host one round with me first, then give me host again.")
			next(6, 2*time.Second)
			return
		}
		chat(send, "I'm the host now! Test match: first a song nobody has (you should see \"no song\"), then the last song we played.")
		send("hover", map[string]any{"pv": missingPV, "title": missingTitle})
		next(1, 6*time.Second)
	case 1:
		log.Printf("host mode: starting the missing song")
		send("start", withPV(lastStart, missingPV, missingTitle))
		next(2, time.Second)
	case 2:
		if roundOver() {
			next(3, 3*time.Second)
		} else {
			h.at = now.Add(500 * time.Millisecond)
		}
	case 3:
		pv := int(lastStart["pv"].(float64))
		title, _ := lastStart["title"].(string)
		send("hover", map[string]any{"pv": pv, "title": title})
		chat(send, "Next: "+title+". Tick Ready on song select and it starts by itself, with your own outfits!")
		next(4, 4*time.Second)
	case 4:
		if room.State == "playing" {
			next(5, time.Second)
			return
		}
		send("start", lastStart) // refused until everyone is ready; try again
		h.at = now.Add(3 * time.Second)
	case 5:
		if roundOver() || room.State == "lobby" {
			next(6, 3*time.Second)
		} else {
			h.at = now.Add(500 * time.Millisecond)
		}
	case 6:
		for _, p := range room.Players {
			if p.ID != myID {
				chat(send, "Thanks for testing! The host is yours again.")
				send("host", map[string]any{"id": p.ID})
				break
			}
		}
		next(7, time.Hour) // done until we are given host again
	}
}
