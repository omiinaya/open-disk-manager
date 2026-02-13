#!/usr/bin/env node
// Discord channel cleaner using native fetch (Node 18+)
// Usage: node delete_channel_v2.js --channel <CHANNEL_ID>

const config = require('/root/.openclaw/openclaw.json');

const args = process.argv.slice(2);
const channelIdx = args.indexOf('--channel');
if (channelIdx === -1 || channelIdx === args.length - 1) {
  console.error('Usage: node delete_channel_v2.js --channel <CHANNEL_ID>');
  process.exit(1);
}
const channelId = args[channelIdx + 1];

const token = config.channels?.discord?.accounts?.default?.token;
if (!token) {
  console.error('Discord token not found in config');
  process.exit(1);
}

async function sleep(ms) {
  return new Promise(resolve => setTimeout(resolve, ms));
}

async function deleteChannel() {
  let lastId = null;
  let totalDeleted = 0;
  let retryAfter = 0;

  while (true) {
    // Build URL for fetching messages
    let url = `https://discord.com/api/v10/channels/${channelId}/messages?limit=100`;
    if (lastId) {
      url += `&before=${lastId}`;
    }

    // Fetch messages
    const headers = {
      'Authorization': `Bot ${token}`,
      'Content-Type': 'application/json',
    };

    let resp;
    try {
      resp = await fetch(url, { headers });
    } catch (err) {
      console.error(`Fetch error: ${err.message}`);
      break;
    }

    if (resp.status === 429) {
      const retry = parseInt(resp.headers.get('Retry-After') || '5', 10);
      console.log(`Rate limited, waiting ${retry}s...`);
      await sleep(retry * 1000);
      continue;
    }

    if (resp.status === 404) {
      console.error('Channel not found or bot lacks access.');
      break;
    }

    if (!resp.ok) {
      console.error(`HTTP ${resp.status}: ${resp.statusText}`);
      break;
    }

    const messages = await resp.json();
    if (!Array.isArray(messages) || messages.length === 0) {
      console.log('No more messages.');
      break;
    }

    console.log(`Processing ${messages.length} messages...`);

    for (const msg of messages) {
      if (msg.type === 1 || msg.type === 20 || msg.type === 22) {
        // Skip certain message types (system, etc.)
        continue;
      }

      const delUrl = `https://discord.com/api/v10/channels/${channelId}/messages/${msg.id}`;
      let delResp;
      try {
        delResp = await fetch(delUrl, { method: 'DELETE', headers });
      } catch (err) {
        console.error(`  delete error (${msg.id}): ${err.message}`);
        continue;
      }

      if (delResp.status === 429) {
        const retry = parseInt(delResp.headers.get('Retry-After') || '2', 10);
        console.log(`  rate limited on delete, waiting ${retry}s...`);
        await sleep(retry * 1000);
        // retry this message
        continue;
      }

      if (delResp.ok) {
        totalDeleted++;
        console.log(`  deleted ${msg.id}`);
      } else {
        console.log(`  failed delete ${msg.id}: ${delResp.status} ${delResp.statusText}`);
      }

      // Small delay to avoid hitting rate limits
      await sleep(250);
    }

    lastId = messages[messages.length - 1].id;
  }

  console.log(`\nTotal deleted: ${totalDeleted}`);
}

deleteChannel().catch(console.error);
