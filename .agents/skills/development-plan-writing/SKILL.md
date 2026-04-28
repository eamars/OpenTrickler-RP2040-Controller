---
name: development-plan-writing
description: Write, review, or improve execution-ready development plans for multi-file changes, migrations, decommissions, architecture changes, prompt/LLM pipeline changes, database changes, or risky refactors. Use this skill whenever the user asks for a development plan, implementation plan, refactor plan, migration plan, decommission plan, or asks you to evaluate whether a plan is clear enough for an AI agent to execute. The plan must serve both a human reader and an implementation agent, resolve decisions before finalizing, state cutover policy explicitly, and prevent agents from inventing scope.
---

# Development Plan Writing

Use this skill to create development plans that are pleasant for a human to read and precise enough for an AI implementation agent to follow without making architectural decisions on its own.

A final development plan is not a brainstorming document. It is an approved work contract.

## Core Standard

Every final plan must satisfy two audiences:

- **Human owner:** can quickly understand the goal, risk, strategy, scope, and evidence for completion.
- **Implementation agent:** can follow exact technical instructions, file paths, contracts, order, forbidden changes, and verification gates without inventing missing decisions.

If those audiences conflict, split the writing into human-readable rationale first and agent-executable instructions second. Do not bury decisions only inside prose.

## Filename Rule

Development plan filenames must be all lowercase.

Prefer snake_case:

```text
rag1_decommission_plan.md
cache2_invalidation_migration_plan.md
```

Do not use uppercase letters in plan filenames:

```text
RAG1_decommission_plan.md
RAG_SUPERVISOR2_PLAN.md
```

## Plan Lifecycle

Keep these stages separate:

- **Discovery / Drafting:** questions, options, code inspection, tradeoffs, and user confirmation are allowed here.
- **Final Development Plan:** no unresolved questions, no decision prompts, no alternatives left for the implementation agent.
- **Execution Record:** what was done, what passed, what failed, and evidence.

Do not mix final-plan content with unresolved design discussion.

## Required Top Matter

Start each plan with a compact human-readable summary:

```md
# <lowercase plan title>

## Summary

- Goal:
- Plan class:
- Status:
- Overall cutover strategy:
- Highest-risk areas:
- Acceptance criteria:
```

Use one of these status values:

```text
draft | approved | in_progress | completed | superseded
```

Use one of these plan classes:

```text
small | medium | large | high_risk_migration
```

## Length Budget

Choose the plan class before writing.

| Plan class | Typical scope | Target length | Maximum length |
|---|---|---:|---:|
| small | 1-2 files, low risk | 80-150 lines | 200 lines |
| medium | several files, no data migration | 150-300 lines | 400 lines |
| large | many files, contracts/prompts/tests | 300-600 lines | 800 lines |
| high_risk_migration | deletion, DB work, production behavior change | 500-900 lines | 1200 lines |

If a plan exceeds the target, compress repetition, move examples to appendices, or reference existing docs. Do not remove mandatory rules, scope, contracts, cutover policy, implementation order, or verification gates just to meet the length budget.

## Mandatory Sections

Every final plan for a non-trivial change must include these sections:

```md
## Summary
## Context
## Mandatory Rules
## Must Do
## Deferred
## Cutover Policy
## Agent Autonomy Boundaries
## Target State
## Design Decisions
## Change Surface
## Implementation Order
## Verification
## Acceptance Criteria
```

Add these sections whenever relevant:

```md
## Data Migration
## Rollback / Recovery
## Risks
## Operational Steps
## Execution Evidence
## Glossary
```

## Mandatory Rules

Each plan must explicitly state the project-specific rules the agent must follow. Do not rely on the agent loading skills, memories, or repo conventions efficiently.

Include rules such as:

- coding style rules
- test execution rules
- prompt safety rules
- trusted vs untrusted prompt boundaries
- database safety rules
- forbidden filtering or validation patterns
- required skill-derived practices, copied into the plan

Write the rules directly in the plan. It is acceptable and often desirable to duplicate important skill content here.

## No Unresolved Questions

Final plans must not contain unresolved questions or decision points.

Avoid these in final plans:

- `TBD`
- `maybe`
- `consider`
- `choose one`
- `option A / option B`
- `ask the user whether...`
- open-ended recommendations

Resolve uncertainty before finalizing the plan. If a decision depends on code inspection, inspect the code first. If a decision depends on user preference, ask the user during discovery, then encode the confirmed decision as an instruction.

Assumptions are allowed, but they must be fixed operating inputs:

```md
## Assumptions

- RAG2 is the only supported retrieval path after this refactor.
- Compatibility with the RAG1 state shape is intentionally not preserved.
- Legacy MongoDB collections may be dropped through the approved migration path.
```

Do not write assumptions as disguised questions.

## Must Do

The `Must Do` section defines non-negotiable scope.

Use directive language:

```md
## Must Do

- Replace all `research_facts` consumers with `rag_result`.
- Delete RAG1 modules listed in this plan.
- Add projection and invalidation tests listed in this plan.
- Run every verification command in the Verification section.
```

The implementation agent must not downgrade, reinterpret, or skip these items.

## Deferred

The `Deferred` section defines explicit non-scope.

Use directive language:

```md
## Deferred

- Do not redesign RAG2 helper-agent routing.
- Do not add persistent Cache2 storage.
- Do not create compatibility shims for the old RAG1 state shape.
- Do not refactor unrelated prompt architecture.
```

The implementation agent must not opportunistically do deferred work, even if it looks useful.

## Agent Autonomy Boundaries

Add a section that constrains implementation-agent judgment.

Recommended language:

```md
## Agent Autonomy Boundaries

- The agent may choose local implementation mechanics only when they preserve the contracts in this plan.
- The agent must not introduce new architecture, alternate migration strategies, compatibility layers, fallback paths, or extra features.
- The agent must not perform unrelated cleanup, formatting churn, dependency upgrades, prompt rewrites, or broad refactors unless explicitly listed in Must Do.
- If the plan and code disagree, the agent must preserve the plan's stated intent and report the discrepancy.
- If a required instruction is impossible, the agent must stop and report the blocker instead of inventing a substitute.
```

## Cutover Policy

Every plan that changes existing behavior must define cutover policy. For broad plans, use a policy matrix per area.

Each area must be marked exactly one of:

- `bigbang`
- `migration`
- `compatible`

The policy must be confirmed with the user before finalizing the plan.

### Strategy Definitions

Use these definitions verbatim or close to verbatim:

- **migration:** Move from old behavior to new behavior through explicit transitional steps. Temporary coexistence is allowed only where the plan says so. Data migration or backfill may be required. Old paths are removed after migration is verified.
- **compatible:** Preserve old and new behavior at the same time. Compatibility shims, adapters, fallback paths, dual reads/writes, or old API/state shapes are allowed only if explicitly listed.
- **bigbang:** Replace old behavior with new behavior in one cutover. No compatibility shims, no adapters, no fallback to old behavior, no dual path, and no preservation of old state/API shapes unless explicitly listed as retained.

### Policy Matrix

Use a table like this:

```md
## Cutover Policy

Overall strategy: bigbang

| Area | Policy | Instruction |
|---|---|---|
| RAG supervisor entrypoint | bigbang | Replace RAG1 with RAG2 directly. No fallback. |
| RAG state shape | bigbang | Remove `research_facts` and `research_metadata`. Do not preserve old state. |
| MongoDB legacy collections | migration | Drop through the approved migration path. Do not delete ad hoc. |
| Tests | bigbang | Delete obsolete RAG1 tests and create RAG2 replacement tests. |
```

If a local area policy conflicts with the overall strategy, the local area policy wins.

### Enforcement

Include enforcement language:

```md
## Cutover Policy Enforcement

- The implementation agent must follow the selected policy for each area.
- The agent must not choose a more conservative strategy by default.
- If an area is `bigbang`, delete or rewrite legacy references instead of preserving them.
- If an area is `migration`, follow the exact migration phases and cleanup gates listed in this plan.
- If an area is `compatible`, preserve only the compatibility surfaces explicitly listed in this plan.
- Any change to a cutover policy requires user approval before implementation.
```

## Context And Target State

Describe the current state, target state, and why the change exists.

Good context includes:

- old architecture and new architecture
- exact state/data shape changes
- production vs test-only status
- known consumers
- external systems affected
- why legacy behavior is being removed or preserved

Target state should describe observable end behavior, not just files changed.

## Design Decisions

Use a decision table for meaningful choices:

```md
## Design Decisions

| Topic | Decision | Rationale |
|---|---|---|
| Cache invalidation | Use Cache2 dependency events | Cache1 version counters are removed. |
| State payload | Use hybrid `rag_result` | Structured image data is needed, raw search blobs are too large. |
```

Only include settled decisions. Do not list alternatives unless the alternative is clearly rejected and the rejection helps prevent future agent drift.

## Contracts And Data Shapes

For architecture or pipeline changes, define the new contracts explicitly:

- function signatures
- state keys
- payload shape
- input/output cardinality
- ownership boundaries
- refusal or failure conditions
- latency or call-count expectations, if relevant

Prefer precise examples:

```python
{
    "answer": str,
    "user_image": dict,
    "conversation_evidence": list[str],
    "supervisor_trace": {
        "loop_count": int,
        "unknown_slots": list[str],
    },
}
```

Define forbidden compatibility shapes when relevant:

```md
Do not preserve or recreate the legacy `research_facts` / `research_metadata` payload.
```

## Change Surface

Separate files into clear groups:

```md
## Change Surface

### Delete
### Modify
### Create
### Keep
```

For each path, explain why it is in that group. Use stable file paths and symbols. Line numbers may be included as hints, but never rely on line numbers alone.

## Implementation Order

Implementation order must prevent avoidable breakage and agent improvisation.

Good ordering:

- create the new contract first
- rewrite consumers next
- wire the new entrypoint
- add or update tests
- delete legacy modules after references are gone
- run greps and smoke tests last

Include a short rationale when order matters:

```md
Build the projection module first because it becomes the contract used by cognition, consolidation, and tests.
```

## Verification

Verification must be written as gates, not vague reminders.

Include exact commands or checks:

```md
## Verification

### Static Greps

- `rg "research_facts|research_metadata" src` returns no matches.

### Tests

- `pytest tests/test_rag_projection.py`
- `pytest tests/test_save_conversation_invalidation.py`

### Smoke

- Service boots without missing import errors.
- One chat request returns a non-empty response.

### Database

- `rag_cache_index` and `rag_metadata_index` are absent after migration.
```

State allowed exceptions directly beside each check.

## Acceptance Criteria

Acceptance criteria describe the observable completed state.

Example:

```md
## Acceptance Criteria

This plan is complete when:

- There is exactly one active RAG path.
- Legacy RAG1 modules and Cache1 modules are deleted.
- No source file imports Cache1 or RAG1.
- New tests for projection and Cache2 invalidation pass.
- Legacy MongoDB collections are absent through the approved migration path.
```

## Rollback / Recovery

Required for destructive or production-affecting plans.

Include:

```md
## Rollback / Recovery

- Code rollback path:
- Data rollback path:
- Irreversible operations:
- Required backup:
- Recovery verification:
```

If rollback is impossible after a step, say so plainly and include the required precondition, such as a database backup.

## Risks

Use a compact risk table:

```md
## Risks

| Risk | Mitigation | Verification |
|---|---|---|
| Stale cache after conversation save | Emit Cache2 event in `save_conversation` | Cache invalidation test and live smoke |
```

## Execution Evidence

Do not treat checked boxes as proof. If the plan also records completion, add a separate evidence section:

```md
## Execution Evidence

- Static grep results:
- Test results:
- Service boot:
- DB verification:
- Manual smoke:
```

Pre-execution plans should use unchecked checklist items. If a plan is completed, either move checked items into an execution record or attach evidence for the checks.

## Writing Style

Write for a smart human and a literal implementation agent.

Use:

- direct instructions
- stable names and paths
- short rationale before long checklists
- explicit scope boundaries
- exact verification gates

Avoid:

- hidden decisions in prose
- ambiguous safety language
- optimistic phrases without tests
- stale line-number-only references
- long duplicated checklists when a table would be clearer
- recommendations that are not accepted instructions

The best development plans make the correct path easy and the wrong path obviously out of bounds.
