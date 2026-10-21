// bot is a fake MultiDiva player for testing alone: it joins a room, readies up, streams
// made-up scores during a round and finishes shortly after the real players do.
package main

import (
	"context"
	_ "embed"
	"encoding/base64"
	"encoding/json"
	"flag"
	"log"
	"math/rand"
	"os"
	"time"

	"github.com/coder/websocket"
)

// The bot's default picture (128x128), sent as a custom avatar.
//
//go:embed miku.png
var mikuAvatar []byte

type envelope struct {
	Type string          `json:"type"`
	Data json.RawMessage `json:"data,omitempty"`
}

func main() {
	url := flag.String("url", "ws://127.0.0.1:3939/ws", "server URL")
	name := flag.String("name", "Miku Bot", "player name")
	room := flag.String("room", "miku", "room to join")
	skill := flag.Int("skill", 160, "average points per 100 ms tick (160 ~ 350k over a 3.5 minute song)")
	rival := flag.Bool("rival", false, "stay close to the best real player's score (lead changes for testing the leaderboard)")
	steamID := flag.String("steam", "", "Steam ID to show the bot with that user's avatar")
	avatarPath := flag.String("avatar", "", "PNG/JPEG avatar (max 32 KB, 256x256) instead of the built-in Miku; \"none\" for no avatar")
	loadDelay := flag.Duration("load-delay", time.Second, "how long the bot pretends to load a song")
	flag.Parse()

	avatar := mikuAvatar
	switch *avatarPath {
	case "":
	case "none":
		avatar = nil
	default:
		var err error
		if avatar, err = os.ReadFile(*avatarPath); err != nil {
			log.Fatalf("avatar: %v", err)
		}
	}

	ctx := context.Background()
	conn, _, err := websocket.Dial(ctx, *url, nil)
	if err != nil {
		log.Fatalf("dial: %v", err)
	}
	defer conn.CloseNow()

	send := func(typ string, data any) {
		raw, _ := json.Marshal(data)
		msg, _ := json.Marshal(envelope{Type: typ, Data: raw})
		if err := conn.Write(ctx, websocket.MessageText, msg); err != nil {
			log.Fatalf("write: %v", err)
		}
	}

	send("hello", map[string]any{"name": *name, "version": 1, "steam": *steamID,
		"avatar": base64.StdEncoding.EncodeToString(avatar)})
	send("join", map[string]any{"room": *room})
	send("ready", map[string]any{"ready": true})

	myID := -1 // from `welcome`; names can repeat, ids cannot
	var playing, othersDone bool
	best, gap := 0, 0 // rival mode: best real score seen, and our drifting distance to it
	score, combo := 0, 0
	completion, bestCompletion := 0.0, 0.0 // clear %, ours and the best real player's
	tick := time.NewTicker(100 * time.Millisecond)
	defer tick.Stop()
	incoming := make(chan envelope)
	go func() {
		for {
			_, data, err := conn.Read(ctx)
			if err != nil {
				log.Fatalf("read: %v", err)
			}
			var env envelope
			if json.Unmarshal(data, &env) == nil {
				incoming <- env
			}
		}
	}()

	// Host mode: given host, the bot runs a scripted test match for players testing alone (see hostScript).
	var roomNow roomState
	var lastStart map[string]any // the last real round's pick (block, pv, ...), replayed in host mode
	script := hostScript{}
	var goAt time.Time

	var doneAt time.Time
	for {
		select {
		case env := <-incoming:
			switch env.Type {
			case "welcome":
				var w struct {
					ID int `json:"id"`
				}
				json.Unmarshal(env.Data, &w)
				myID = w.ID
			case "room":
				json.Unmarshal(env.Data, &roomNow)
			case "start":
				var pick map[string]any
				if json.Unmarshal(env.Data, &pick) == nil && pick["pv"] != float64(missingPV) {
					lastStart = pick
				}
				log.Printf("round started, loading")
				othersDone, score, combo, best, gap = false, 0, 0, 0, 0
				completion, bestCompletion = 0, 0
				// Pretend to load, then wait at the barrier for `go`.
				time.AfterFunc(*loadDelay, func() { send("loaded", map[string]any{}) })
			case "go":
				log.Printf("go")
				playing, goAt = true, time.Now()
			case "scores":
				var s struct {
					Players []struct {
						ID         int     `json:"id"`
						Score      int     `json:"score"`
						Completion float64 `json:"completion"`
						Finished   bool    `json:"finished"`
					} `json:"players"`
				}
				json.Unmarshal(env.Data, &s)
				all, others := true, 0
				best, bestCompletion = 0, 0
				for _, p := range s.Players {
					if p.ID != myID {
						others++
					}
					if p.ID != myID && !p.Finished {
						all = false
					}
					if p.ID != myID && p.Score > best {
						best = p.Score
					}
					if p.ID != myID && p.Completion > bestCompletion {
						bestCompletion = p.Completion
					}
				}
				if playing && all && others > 0 && !othersDone {
					othersDone, doneAt = true, time.Now().Add(time.Second)
				}
				// Playing alone (everyone else sat out): a short round.
				if playing && others == 0 && !othersDone && time.Since(goAt) > soloRound {
					othersDone, doneAt = true, time.Now()
				}
			case "results":
				log.Printf("results: %s", env.Data)
				playing = false
				send("ready", map[string]any{"ready": true})
			case "chat":
				log.Printf("chat: %s", env.Data)
			case "error":
				log.Printf("error: %s", env.Data)
			case "kicked":
				// The host removed the bot: quit, so the host's "Add Miku Bot" button comes back.
				log.Printf("kicked: %s", env.Data)
				conn.Close(websocket.StatusNormalClosure, "kicked")
				os.Exit(0)
			}
		case <-tick.C:
			script.step(send, roomNow, myID, lastStart)
			if !playing {
				continue
			}
			if othersDone && time.Now().After(doneAt) {
				playing = false
				send("finish", map[string]any{"result": map[string]any{
					"score": score, "combo": combo, "maxCombo": combo, "completion": completion, "grade": 3,
					"cool": combo, "fine": 12, "safe": 3, "sad": 1, "worst": 2,
				}})
				continue
			}
			if rand.Intn(120) == 0 {
				combo = 0 // the occasional combo break
			} else {
				combo++
			}
			if *rival {
				// Shadow the best real player with a drifting gap, so the lead keeps changing hands.
				// Scores never go down, so falling behind just means not scoring for a moment.
				gap += rand.Intn(1601) - 800
				if gap > 12000 || gap < -12000 {
					gap /= 2
				}
				if target := best + gap; target > score {
					score = target
				}
			} else {
				score += *skill/2 + rand.Intn(*skill)
			}
			if *rival {
				// Hover around the best real player's clear %, never going down.
				if target := bestCompletion + float64(gap)/4000; target > completion {
					completion = target
				}
			} else {
				completion += 0.04 + rand.Float64()*0.04
			}
			completion = min(completion, 100)
			send("score", map[string]any{"score": score, "combo": combo, "completion": completion})
		}
	}
}
