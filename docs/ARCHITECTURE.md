<!-- Architecture overview for the Patina repository -->
# Patina Architecture

This document provides a high-level architecture diagram of the Patina repository.

```mermaid
flowchart LR
  repo["Patina Repository"]
  dsp["dsp/\n(core, engine, circuits, parts)"]
  bindings["bindings/\n(c, rust)"]
  apps["apps/\n(demos, JUCE apps)"]
  examples["examples/\n(example programs)"]
  include["include/\n(public headers)"]
  docs["docs/\n(guide & API docs)"]
  tests["tests/\n(unit & integration)"]

  repo --> dsp
  repo --> bindings
  repo --> apps
  repo --> examples
  repo --> include
  repo --> docs
  repo --> tests

  dsp --> engine["Engine (engine/*)"]
  dsp --> circuits["Circuits (circuits/*)"]
  dsp --> parts["Parts (parts/*)"]
  bindings --> cbind["C bindings"]
  bindings --> rustbind["Rust bindings"]

  style repo fill:#f9f,stroke:#333,stroke-width:1px
```

Notes:

- Keep `CHANGELOG.md` and other internal-only files in the private repository.
- This diagram is intentionally high-level; expand per-component diagrams where needed.
