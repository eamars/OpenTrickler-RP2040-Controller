---
name: py-style
description: Enforce and review Python coding style for this project. Use this skill whenever you are writing new Python code, reviewing existing Python files, or responding to feedback about code quality. Trigger on any task that involves creating or modifying .py files, reviewing a function or class, or when the user asks about code style, best practices, or refactoring. Always apply these rules proactively — don't wait for the user to ask.
---

# Python Style Guide

This skill defines and enforces the project's six coding standards. Apply them proactively when writing Python code, and surface every violation when reviewing code.

The rules exist to keep code readable, debuggable, and honest about failure. Clever workarounds that hide bugs are more expensive than crashes that reveal them immediately.

---

## Rule 1 — Imports at the top of the module

All `import` and `from … import` statements belong at the top of the file, grouped after the module docstring. Inline imports (inside functions, methods, or class bodies) obscure dependencies and slow down readers who need to understand what the module requires.

**The only exception:** test code — `test_main()` bodies, pytest fixtures/functions, and `if __name__ == "__main__"` blocks — may use inline imports when they need to pull in heavy or optional dependencies without affecting the module's public load cost.

**Wrong:**
```python
async def save_memory(doc):
    from datetime import datetime  # hidden dependency
    ...
```

**Right:**
```python
from datetime import datetime  # top of file, visible to all readers

async def save_memory(doc):
    ...
```

When reviewing: scan every function and method body. Flag any `import` statement that is not inside test/harness code.

---

## Rule 2 — `try` blocks contain only the risky statement(s)

A `try` block answers the question "what specifically might fail here?" Its scope must be limited to exactly that. Logging, assignment to local variables, calling unrelated functions — none of these belong inside `try`. Keeping the block narrow prevents accidentally catching exceptions from the wrong place, which creates silent, hard-to-diagnose bugs.

**Wrong:**
```python
try:
    result = parse_llm_output(response)
    logger.info("Parsed %d facts", len(result))   # cannot raise ParseError
    enriched = enrich(result)                      # unrelated concern
except ParseError:
    result = {}
```

**Right:**
```python
try:
    result = parse_llm_output(response)
except ParseError:
    result = {}
logger.info("Parsed %d facts", len(result))
enriched = enrich(result)
```

When reviewing: for each `try` block, verify every statement inside is directly capable of raising the caught exception. Move anything else outside.

---

## Rule 3 — `except` always names a specific exception type

Bare `except:` and `except Exception:` hide the nature of the failure. They catch `KeyboardInterrupt`, `SystemExit`, memory errors, and every other Python exception — usually not what was intended. Naming the exact exception type is a contract: it says "I expect this specific failure, and I know why."

Broad catches are only acceptable at genuine process boundaries (e.g. a top-level Discord event handler or a service main loop) where crashing the entire process would be worse than logging and continuing. Those sites must log with `logger.exception(...)` to preserve the full traceback.

**Wrong:**
```python
try:
    value = int(user_input)
except:             # what error? why?
    value = 0
```

**Wrong:**
```python
except Exception:   # too broad for application logic
    ...
```

**Right:**
```python
try:
    value = int(user_input)
except ValueError:  # specific: int() raises ValueError for bad input
    value = 0
```

When reviewing: flag every `except Exception` and bare `except`. Accept them only at genuine process boundaries that log `logger.exception(...)`.

---

## Rule 4 — Every non-trivial function has a docstring with purpose, args, and return value

Type annotations tell the interpreter what types are expected. Docstrings tell humans *why* a function exists, *what* each argument means conceptually, and *what* the return value represents. A reader should be able to understand a function's contract without reading its body.

Skip the docstring only for one-liner helpers whose name already makes the contract self-evident (e.g. a property that returns a stored attribute).

**Wrong:**
```python
async def update_affinity(global_user_id: str, delta: int) -> int:
    ...
```

**Wrong (too thin):**
```python
async def update_affinity(global_user_id: str, delta: int) -> int:
    """Update affinity."""
    ...
```

**Right:**
```python
async def update_affinity(global_user_id: str, delta: int) -> int:
    """Apply a signed delta to the user's affinity score, clamped to 0–1000.

    Args:
        global_user_id: Internal UUID identifying the user in user_profiles.
        delta: Signed integer; positive values increase affinity, negative decrease.
            The caller is responsible for scaling the raw delta before passing it.

    Returns:
        The new affinity value after clamping.
    """
    ...
```

When reviewing: check every public function and method (those not prefixed with `_` or trivially obvious). Flag missing docstrings and docstrings missing Args or Returns sections.

---

## Rule 5 — Default values live in one place; `.get(key, default)` is not a general substitute

When the same default value appears at every call site via `.get(key, fallback)`, two problems arise: the default can silently diverge across sites, and the code masks the fact that the data was never written in the first place. The right approach is to assign defaults at the point of definition — the config constant, the DB write, the TypedDict — so that the read site can use plain indexing and crash immediately if the value is absent. A crash at the read site means the bug is upstream; surfacing it is the goal.

`.get(key, fallback)` *is* appropriate when reading from genuinely external or untrusted sources: LLM JSON output, API responses, user-provided dicts, or config keys that are intentionally optional.

**Wrong — scattered defaults:**
```python
affinity = doc.get("affinity", 500)       # site 1
affinity = state.get("affinity", 500)     # site 2 — what if it diverges?
affinity = profile.get("affinity", 500)   # site 3
```

**Right — one source of truth:**
```python
# config.py
AFFINITY_DEFAULT = 500

# db.py — the write sets the default once
new_profile = {"affinity": AFFINITY_DEFAULT, ...}
await db.user_profiles.insert_one(new_profile)

# everywhere else — plain indexing; crash if missing signals a write bug
affinity = doc["affinity"]
```

**Acceptable `.get()` uses:**
```python
# LLM output is external and may be incomplete
depth = parsed_llm_output.get("depth", "DEEP")

# Genuinely optional config key
debug = config.get("DEBUG_MODE", False)
```

When reviewing: for each `.get(key, value)` call on an internal dict, ask whether the fallback papers over a missing upstream write. If yes, flag it.

---

## Rule 6 — `try-except` is not default error handling; crashes are informative

If internal code crashes, that crash is a bug report — it tells you exactly where the assumption was wrong. Wrapping internal logic in `try-except` silences the report and delays the diagnosis. Code is *expected* to crash when called incorrectly; the crash surfaces the real bug.

`try-except` is justified only when the failure source is genuinely outside the codebase: LLM output (which is structurally unpredictable), network I/O (where transient failures are normal), external APIs, file system operations on user-supplied paths, and OS-level calls.

**Wrong — suppressing internal bugs:**
```python
try:
    result = compute_embedding(text)
except Exception:
    result = []          # embedding service is expected to work; this hides failures
    logger.warning("embedding failed")
```

**Wrong — using try-except to handle a missing key:**
```python
try:
    user_id = state["global_user_id"]
except KeyError:
    user_id = "anonymous"   # fix the caller instead; the key should always be there
```

**Wrong — defensive DB wrap:**
```python
try:
    await db.user_profiles.update_one(...)
except Exception:
    pass    # DB is internal; failures should propagate
```

**Right — wrapping only genuine external uncertainty:**
```python
# LLM output is external; malformed JSON is a realistic and expected outcome
try:
    parsed = json.loads(llm_response)
except json.JSONDecodeError:
    logger.exception("LLM returned non-JSON: %r", llm_response)
    parsed = {}

# Network I/O where transient failure is a normal operating condition
try:
    resp = await embedding_client.embeddings.create(input=[text], model=model)
except (TimeoutError, httpx.ConnectError) as exc:
    raise EmbeddingUnavailableError(f"Embedding service unreachable: {exc}") from exc
```

When reviewing: for each `try-except`, ask whether the caught exception is caused by something outside the current codebase. If the only way it fires is a bug in internal code, remove the block and let it crash.

---

## Review workflow

When asked to review a file or selection:

1. Read the code in full.
2. For each rule, list every violation with the line reference, the rule number, and a one-sentence explanation of why it violates the rule.
3. Propose the corrected version inline.
4. If no violations are found, say so explicitly.

When writing new code:

Apply all six rules before producing any output. If a rule conflict arises or a genuine exception is needed, surface it to the user with the reasoning rather than silently picking one path.

See `references/examples.md` for additional annotated before/after examples.
