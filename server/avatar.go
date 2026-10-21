package main

import (
	"bytes"
	"encoding/base64"
	"image"
	_ "image/jpeg" // registers the decoders image.DecodeConfig uses
	_ "image/png"
	"net/http"
	"strconv"
	"strings"
)

// Custom avatars, for players without a Steam one (MikuBot): a small PNG or JPEG sent in `hello`,
// served back to the other players at /avatar/<player id>.
const (
	MaxAvatarBytes = 32 << 10
	MaxAvatarSide  = 256
)

type Avatar struct {
	data        []byte
	contentType string
}

// parseAvatar accepts base64 of a PNG/JPEG no larger than MaxAvatarBytes and MaxAvatarSide pixels a side.
func parseAvatar(b64 string) (Avatar, bool) {
	if b64 == "" || base64.StdEncoding.DecodedLen(len(b64)) > MaxAvatarBytes+3 {
		return Avatar{}, false
	}
	data, err := base64.StdEncoding.DecodeString(b64)
	if err != nil || len(data) > MaxAvatarBytes {
		return Avatar{}, false
	}
	cfg, format, err := image.DecodeConfig(bytes.NewReader(data))
	if err != nil || cfg.Width <= 0 || cfg.Height <= 0 || cfg.Width > MaxAvatarSide || cfg.Height > MaxAvatarSide {
		return Avatar{}, false
	}
	return Avatar{data: data, contentType: "image/" + format}, true
}

func (h *Hub) setAvatar(id int, a *Avatar) {
	h.mu.Lock()
	defer h.mu.Unlock()
	if a == nil {
		delete(h.avatars, id)
	} else {
		h.avatars[id] = *a
	}
}

func (h *Hub) ServeAvatar(w http.ResponseWriter, r *http.Request) {
	id, err := strconv.Atoi(strings.TrimPrefix(r.URL.Path, "/avatar/"))
	h.mu.Lock()
	a, ok := h.avatars[id]
	h.mu.Unlock()
	if err != nil || !ok {
		http.NotFound(w, r)
		return
	}
	w.Header().Set("Content-Type", a.contentType)
	w.Header().Set("X-Content-Type-Options", "nosniff")
	w.Write(a.data)
}
