# Report Worker

Receives the mod's bug and crash reports ("Report a bug" in F8, and the prompt after a crash) and posts each one
to a private Discord channel, with the logs attached. Players only send a report when they click Send.

## Setup

1. Discord: in a private channel, Edit Channel → Integrations → Webhooks → New Webhook → Copy Webhook URL.
2. Cloudflare dashboard: Workers & Pages → Create → Worker. Name it (e.g. `multidiva2-reports`), deploy, then
   Edit code: replace everything with `worker.js` and deploy again.
3. The Worker's Settings → Variables and Secrets → add a **Secret** named `DISCORD_WEBHOOK` with the webhook URL.
4. Put the Worker's address plus `/report` (e.g. `https://multidiva2-reports.<you>.workers.dev/report`) into
   `ReportUrl` in `client/src/net/report.cpp` and rebuild. An empty `ReportUrl` turns reporting off.

The webhook URL is a secret: it only lives in the Worker's settings, never in this repo.
