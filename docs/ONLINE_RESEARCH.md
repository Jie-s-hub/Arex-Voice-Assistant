# AURA Online Search and Research Mode

AURA can use online search through the PC server. The ESP32 does not search the web directly and does not store the OpenAI API key.

## What changed

The PC server now gives the OpenAI Responses API access to the hosted `web_search` tool.

This means AURA can answer questions such as:

- "What is the weather today?"
- "Search online for recent robotics news."
- "Research the latest smart home safety ideas."
- "What happened in technology this week?"
- "Find information about ACCCIM STI Competition 2026."

Smart-home commands still work the same way:

- "Turn on bedroom light"
- "Turn off bedroom light"
- "Turn on fan"
- "Turn off fan"
- "Turn everything off"

## Enable search

Open:

```text
pc_server/.env
```

Add or confirm these lines:

```dotenv
AURA_ENABLE_WEB_SEARCH=true
AURA_WEB_SEARCH_CONTEXT_SIZE=low
```

Recommended values for `AURA_WEB_SEARCH_CONTEXT_SIZE`:

| Value | Use case |
|---|---|
| `low` | Fast voice answers and demos |
| `medium` | Better research quality |
| `high` | Longer research answers, more latency |

For the rover voice demo, use `low` or `medium`.

## Start the server

```powershell
cd "C:\Users\ongyo.LAPTOP-TUSAO5AT\Documents\New project\AURA-Rover\pc_server"
.\.venv\Scripts\Activate.ps1
aura-server
```

## Test with the ESP32 bench firmware

Upload:

```text
firmware/rover_esp32_bench_test
```

Open Serial Monitor at `115200` baud.

Use:

```text
C
```

to test the PC server connection.

Then use:

```text
V
```

to record your voice and test the full OpenAI round trip.

Try asking:

```text
Search online for one recent robotics invention and explain it simply.
```

Expected result:

1. ESP32 records your voice.
2. PC server transcribes it.
3. OpenAI uses web search if needed.
4. PC server generates speech.
5. ESP32 plays the answer.

## Important API key safety

Never put the OpenAI API key in:

- Arduino code
- ESP32 code
- `secrets.h`
- GitHub

The key belongs only in:

```text
pc_server/.env
```

If a real API key was ever committed to GitHub, revoke it and create a new one before your competition demo.

