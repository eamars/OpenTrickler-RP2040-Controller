---
name: python-venv
description: Manage Python virtual environments for the project. Use this skill whenever you need to create, activate, or verify a Python virtual environment, install dependencies, or check if a venv exists. Triggers when setting up a development environment, installing Python packages, running pip install, or any task that requires an isolated Python environment. Always use this skill before installing packages — it ensures the venv is in the correct location and avoids polluting the global Python installation.
---

# Python Virtual Environment Skill

Ensures a consistent Python virtual environment exists at the **project root** before any Python dependencies are installed.

## Venv Location

The virtual environment **must** be located at the project root in a folder named `venv/`:

```
<project_root>/
├── venv/          ← always here
├── workspace/
├── .agents/
└── ...
```

**Never create a venv inside `workspace/`, `.agents/`, or any subdirectory.** The project root is the directory that contains `workspace/` and `.agents/`.

## Workflow

### 1. Check if venv exists

Before creating a new venv, check if one already exists:

```bash
# Windows
Test-Path venv\Scripts\activate

# macOS/Linux
test -f venv/bin/activate
```

- **If it exists** — skip creation. Activate and verify packages (step 3).
- **If it does not exist** — create it (step 2).

### 2. Create the venv

```bash
python -m venv venv
```

Then activate:

- **Windows PowerShell**: `.\venv\Scripts\activate`
- **macOS/Linux**: `source venv/bin/activate`

### 3. Install dependencies

If a `requirements.txt` path is provided (by the calling skill or user), install it:

```bash
pip install -r <path_to_requirements.txt>
```

If multiple skills need different dependencies, install each requirements file. Pip handles deduplication automatically.

### 4. Verify

Run a quick import check to confirm key packages are available:

```bash
python -c "import <expected_package>; print('OK')"
```

## Usage by Other Skills

Other skills should **not** embed venv creation logic. Instead, they should reference this skill:

> **Environment**: use the `python-venv` skill to ensure the venv is set up at the project root before installing dependencies.

When calling this skill from another skill, provide:
- The path to the `requirements.txt` to install (if any)
- The package(s) to verify after installation

## Critical Rules

- **One venv per project root** — do not create multiple venvs.
- **Always check before creating** — never blindly run `python -m venv venv` if one already exists.
- **Project root only** — the venv lives at `<project_root>/venv/`, nowhere else.
- **Never delete an existing venv** without explicit user instruction.
