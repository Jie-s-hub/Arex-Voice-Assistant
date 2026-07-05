# Cognee Memory

Cognee is AURA's local long-term memory layer.

It lets the rover remember safe facts across voice sessions, such as:

- your preferred demo name
- your favorite color
- smart-living project notes
- competition presentation facts
- reminders about the rover setup

The memory runs on the PC server only. The Arduino Uno and ESP32 do not store the memory file.

## Why it is called Cognee

This project uses a lightweight local memory module named **Cognee**, inspired by the idea of a simple AI memory system. It is designed for a high-school robotics demo where reliability matters more than a complex database.

You can later replace it with a heavier graph/vector memory system if the project grows.

## Where memories are stored

By default:

```text
pc_server/data/cognee_memory.json
```

This file is ignored by GitHub because it may contain personal notes.

## Enable memory

Open:

```text
pc_server/.env
```

Confirm:

```dotenv
AURA_ENABLE_COGNEE=true
AURA_COGNEE_MEMORY_PATH=data/cognee_memory.json
```

## Voice commands

### Remember something

Say:

```text
Remember that my favorite color is blue.
```

AURA replies:

```text
I'll remember that my favorite color is blue.
```

Other examples:

```text
Remember that my robot is called AURA.
Remember that my competition project is about smart living.
Save memory: the bedroom light relay is connected to GPIO26.
```

### Recall memories

Say:

```text
What do you remember?
```

or:

```text
What do you know about me?
```

AURA replies with saved memories.

### Use memories in normal questions

After saving:

```text
Remember that my favorite color is blue.
```

You can ask:

```text
What is my favorite color?
```

AURA includes the Cognee memory in the model prompt, so it can answer:

```text
Your favorite color is blue.
```

### Forget one memory

Say:

```text
Forget my favorite color.
```

### Clear all memories

Say:

```text
Forget everything.
```

or:

```text
Clear my memory.
```

## Safety and privacy

Cognee stores only explicit memories. AURA does not silently save every conversation.

This is intentional for a school competition demo:

- safer privacy behavior
- easier explanation to judges
- less chance of storing unwanted personal information

Do not save passwords, API keys, Wi-Fi passwords, or private family information.

## Testing

Run the PC server tests:

```powershell
cd "C:\Users\ongyo.LAPTOP-TUSAO5AT\Documents\New project\AURA-Rover\pc_server"
.\.venv\Scripts\Activate.ps1
python -m pytest
```

Expected result:

```text
passed
```

## Demo script

For a simple live demo:

1. Start the PC server.
2. Press the rover voice button.
3. Say: "Remember that my robot is called AURA."
4. Press the voice button again.
5. Say: "What do you remember?"
6. AURA should say that your robot is called AURA.


