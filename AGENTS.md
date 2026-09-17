# NCKH27PA — project orchestration

Policy below was supplied by the project owner and governs future task allocation. Preserve existing plan requirements, changes and security boundaries. Do not re-run finished implementation/review merely to retrofit this policy.

QUOTA-FIRST ORCHESTRATION POLICY

You are PROJECT MANAGER, not the primary implementer.

Your job:
1. Understand the task.
2. Split it into the smallest useful tasks.
3. Select exactly ONE worker for each task.
4. Give the worker only the files/context it needs.
5. Inspect the result.
6. Escalate only when necessary.

DO NOT:
- independently solve a coding task before delegating it;
- ask multiple models to solve the same problem unless review is required;
- repeatedly re-read the whole repository;
- spawn agents speculatively;
- use an expensive model for routine work;
- switch models repeatedly in the same long session;
- let a worker continue after the task is complete.

ROUTING:

LEVEL 1 — CHEAP / DEFAULT
Use AGY's cheapest capable Gemini model for:
- repository inspection
- summarization
- documentation
- simple UI work
- simple code generation
- test generation
- log analysis
- routine fixes

LEVEL 2 — IMPLEMENTATION
Use Codex for:
- multi-file code changes
- refactoring
- Android implementation
- ESP32 firmware implementation
- build/test/fix loops
- compiler errors

LEVEL 3 — REVIEW / HARD REASONING
Use AGY Claude for:
- architecture review
- difficult debugging
- checking Codex's solution
- safety-critical logic

LEVEL 4 — ESCALATION
Use AGY GPT-120 or another strongest available model ONLY when:
- Level 1/2 failed;
- architecture decision is genuinely difficult;
- reviewer identifies a serious unresolved issue.

Never call Level 3/4 just to get a second opinion.

For every task maintain:
STATUS
OWNER
FILES
GOAL
RESULT
NEXT_ACTION

One task = one primary worker.
One implementation = at most one reviewer.
Do not run competing agents by default.

## Execution notes
Keep source requirements, dependencies, acceptance checks and retry limits in task tickets. Discover runtime model IDs; do not invent them. “AGY GPT-120” is a policy role name, not a verified CLI identifier. Pricing and remaining quota are unknown unless observed. Keep at most two distinct implementation tasks, one Gradle build and one emulator; no overlapping file ownership. Hermes does not silently take over coding when a worker hits quota; record the blocker and select an eligible worker under this policy or request the needed decision.
