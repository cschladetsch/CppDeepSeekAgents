# CppDeepSeek: Building a Local-First Agent Runtime in C++ (with Real Guardrails)

Most agent demos require an API key, break offline, and have no real guardrails. That's fine for a toy, but it fails quickly in real engineering contexts where you need latency control, reliability guarantees, and enforceable policies. I wanted a system I could demo **offline**, with **repeatable guardrails**, and a clean architecture that could switch between local inference and a hosted API without rewriting the app.

This post is a deep-dive into **CppDeepSeek**, a C++20 agent runtime that defaults to local inference (via llama.cpp + GGUF), supports DeepSeek's hosted API as a fallback, and includes an explicit **Logic Gate** to enforce policy. It's intentionally pragmatic, because that's what makes systems ship.

**Platform Support:** Works on Linux/WSL2 with CUDA support. Mac support available via Metal backend.

---

## The Core Goals

I kept the goals simple and testable:

1. **Local-first by default** (no API key required).  
2. **Clear policy enforcement** (Logic Gate that can reject inputs).  
3. **Readable demos** (human-paced, streaming output).  
4. **Reusable model cache** across multiple projects.  
5. **Swappable backends** with a stable interface.

If a design didn't support these, it didn't make the cut.

---

## Architecture Overview

At a high level, the system has three layers:

- **Agent Runtime**: Agents have a system prompt, memory, and a debate loop.  
- **Backend Interface**: A thin abstraction for chat + streaming.  
- **ModelStore**: A shared model cache, designed to be pulled into other repos.

The backend interface is key. It allows the exact same agent code to operate against:

- a **local llama.cpp** model, or  
- a **remote DeepSeek API**.

That's the heart of the "local-first" story: the app is always usable, and you can scale up to hosted results only when it helps.

---

## Local Inference (llama.cpp + GGUF)

Local mode is the default. The system expects a GGUF model at:

```
~/.local/share/deepseek/models/deepseek-r1/model.gguf
```

The build script (`./b`) will download a medium/pro default model automatically if it's missing. That gives a frictionless on-ramp while still keeping everything local.

To keep the local backend clean, I created a `LlamaBackend` class that:

- loads the GGUF model  
- tokenizes prompts  
- runs decode steps  
- returns plain text output  

It feeds into the **same Agent Runtime interface** as the API client.

The key takeaway: local inference is no longer an afterthought. It is a first-class backend.

---

## The Logic Gate (Policy as a First-Class Citizen)

This is not safety theater. The Logic Gate is a real **YES/NO** decision point. It has its own prompt, runs before the debate, and can block inputs outright.

In the demo, I use a "software-engineering-only" gate, which rejects off-topic prompts. That gives a clean, obvious proof that policy enforcement is **built into the flow**, not bolted on.

From a leadership perspective, that's the difference between "interesting demo" and "deployable architecture."

---

## Agent Runtime: Memory + Debate + Streaming

The runtime is intentionally minimal but complete:

- Agents have **system prompts** and **memory**.  
- A debate runs for a configurable number of rounds.  
- Output is **streamed** and formatted for readability.  
- Each response is paused with **ENTER** so humans can follow it.

For a blog or demo, the pacing matters. It's hard to appreciate output if it scrolls by instantly. The pause also makes it easy to screen-record and narrate.

We also added **save/load** for memory to keep continuity between runs. This is small, but powerful: you can demo multi-session behavior without any other infrastructure.

The system now starts with an interactive CLI by default - type a topic and press ENTER to begin a debate. This makes exploration natural without requiring command-line arguments.

---

## ModelStore: Shared Cache Across Projects

This is subtle but important: the model cache is **not** tied to a single repo.

`ModelStore` resolves a global path under:

```
~/.local/share/deepseek/models
```

That means:

- your model downloads are **not duplicated** (saving disk space),  
- multiple projects can share the same GGUF file,  
- and you can standardize a "model home" across your team.

The folder is designed to be split into its own repo later - which makes it easy to reuse this cache strategy everywhere.

For teams working with multiple LLM-based tools, this eliminates the problem of having 5+ copies of the same 4GB model scattered across different project directories.

---

## The Demo Flow (Designed for Humans)

The demo script (`demo.sh`) is optimized for a human viewer:

1. Builds and tests.  
2. Runs a multi-round local debate.  
3. Demonstrates memory persistence.  
4. Shows a policy rejection.  
5. Optionally runs a remote API pass.  

Everything is colored, paced, and easy to follow. It's designed for a demo and a blog post, not just for developer convenience.

---

## The 3-Command Quickstart

If you want to try it:

```bash
./b --deps
./b --demo
```

That's the least-friction path. The model download happens automatically, and everything else just runs.

> **Note:** On WSL2, CUDA must be installed **inside WSL2** (not via `/mnt/c`). The setup script handles this and will fail early if CUDA isn't ready. For CPU-only demos, set `DEMO_NO_CUDA=1`. On Mac, use `./b --demo --metal` to enable Metal acceleration.

---

## What This Enables (Why It's Useful)

For engineering teams, this architecture provides:

- **Offline demos** that are still "real."  
- **Cost discipline** by validating locally before scaling.  
- **Policy enforcement** as a first-class feature.  
- **Backend flexibility** without rewriting code.  
- **Shared model storage** across multiple repos.

It's not a toy. It's a framework you can build on.

---

## Next Steps

If I were to extend this, the natural next steps are:

- Tool-calling and external integrations  
- Persistent memory in a DB  
- A service layer for agent orchestration  
- Splitting `modelstore/` into its own repo  

But the MVP already demonstrates the essential architectural ideas.

---

## MVP Checklist (What "Done" Looks Like)

- One-command build: `./b --deps`  
- Local-first demo: `./demo.sh`  
- Shared model cache in `~/.local/share/deepseek/models/...`  
- Logic Gate rejection works  
- Human-readable pacing (ENTER between responses)  
- Interactive CLI for natural exploration  
- README Quickstart updated  

---

If you want a walkthrough or a deeper technical review, I'm happy to share.  
This is a foundation you can actually ship from.

**Repository:** [github.com/cschladetsch/CppDeepSeekAgents](https://github.com/cschladetsch/CppDeepSeekAgents)

#cpp #ai #agents #llama #localfirst #systems
