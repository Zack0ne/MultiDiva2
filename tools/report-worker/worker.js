// MultiDiva2 report Worker: takes bug and crash reports from the mod and posts them to a private Discord channel.
// The Discord webhook stays here as a secret (DISCORD_WEBHOOK), never in the mod, so nobody can spam it directly.

const MaxBodyBytes = 400 * 1024;
const MaxPerHour = 5; // per IP, per Worker instance: enough to stop a flood, never a real player

const recent = new Map(); // ip -> send times (ms)

function limited(ip) {
  const now = Date.now();
  const times = (recent.get(ip) || []).filter((t) => now - t < 3600_000);
  if (times.length >= MaxPerHour) return true;
  times.push(now);
  recent.set(ip, times);
  return false;
}

const str = (v, max) => (typeof v === "string" ? v : "").slice(0, max);

export default {
  async fetch(request, env) {
    const url = new URL(request.url);
    if (request.method !== "POST" || url.pathname !== "/report") return new Response("not found", { status: 404 });
    if (Number(request.headers.get("content-length") || 0) > MaxBodyBytes) return new Response("too big", { status: 413 });
    if (limited(request.headers.get("cf-connecting-ip") || "?")) return new Response("slow down", { status: 429 });

    let r;
    try {
      const text = await request.text();
      if (text.length > MaxBodyBytes) return new Response("too big", { status: 413 });
      r = JSON.parse(text);
    } catch {
      return new Response("bad report", { status: 400 });
    }

    const version = str(r.version, 20);
    const note = str(r.note, 1000);
    const mods = Array.isArray(r.mods) ? r.mods.map((m) => str(m, 60)).slice(0, 60).join(", ") : "";
    const summary =
      `**${r.afterCrash ? "Crash report" : "Bug report"}** · MultiDiva2 ${version || "?"}\n` +
      (note ? `> ${note.replace(/\n/g, "\n> ")}\n` : "") +
      `Mods: ${mods || "(none listed)"}`;
    const file =
      `--- this session's log ---\n${str(r.log, 200_000)}\n\n` +
      `--- previous session's log ---\n${str(r.oldLog, 200_000)}\n`;

    const form = new FormData();
    form.append(
      "payload_json",
      JSON.stringify({ content: summary.slice(0, 1900), allowed_mentions: { parse: [] } }),
    );
    form.append("files[0]", new Blob([file], { type: "text/plain" }), "logs.txt");
    const sent = await fetch(env.DISCORD_WEBHOOK, { method: "POST", body: form });
    return new Response(sent.ok ? "ok" : "discord refused", { status: sent.ok ? 200 : 502 });
  },
};
