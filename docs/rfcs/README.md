# Game Engine — RFC Index

RFCs (Requests for Comments) document design decisions, feature plans, and
architectural changes for the game engine. Each RFC moves through a pipeline:

```
00-proposals → 01-active → 02-implemented
                         → 03-superseded
                         → 04-rejected
```

## Pipeline

| ID | Title | Status | Path |
|----|-------|--------|------|
| 0000 | Prototype Baseline | Baseline/Implemented | [02-implemented/0000-prototype-baseline.md](02-implemented/0000-prototype-baseline.md) |
| 0001 | Architectural Modularization | Implemented | [02-implemented/0001-architectural-overhaul.md](02-implemented/0001-architectural-overhaul.md) |
| 0002 | Quality Assurance & Testing | Implemented | [02-implemented/0002-testing-suite.md](02-implemented/0002-testing-suite.md) |
| 0003 | CI/CD & Rolling Releases | Implemented | [02-implemented/0003-ci-rolling-release.md](02-implemented/0003-ci-rolling-release.md) |
| 0004 | Robust Linux Gamepad Discovery | Implemented | [02-implemented/0004-robust-gamepad-discovery.md](02-implemented/0004-robust-gamepad-discovery.md) |
| 0005 | Unified Input State | Implemented | [02-implemented/0005-unified-input-state.md](02-implemented/0005-unified-input-state.md) |
| 0006 | Character System Decomposition | Implemented | [02-implemented/0006-character-system-decomposition.md](02-implemented/0006-character-system-decomposition.md) |
| 0007 | Component Type Purity | Proposal | [00-proposals/0007-component-type-purity.md](00-proposals/0007-component-type-purity.md) |

## Workflow

1. **Draft** an RFC from `_TEMPLATE.md` and place it in `00-proposals/`.
2. **Discuss** — iterate on the design until the approach is agreed upon.
3. **Activate** — move to `01-active/` when implementation begins. Only one RFC
   should be active at a time.
4. **Complete** — move to `02-implemented/` when the work is done, tests pass,
   and docs are updated.
5. **Supersede/Reject** — if a proposal is replaced or abandoned, move it to
   `03-superseded/` or `04-rejected/` with a note explaining why.
