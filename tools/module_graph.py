"""The firmware's modules and what each requires, read from their build files (ADR-006).

A module is src/<layer>/<module>/, and its CMakeLists.txt names what it needs: REQUIRES (its headers need it, so
whoever requires the module gets it too) and PRIV_REQUIRES (only its sources do). The layers are ordered, and a module
is to require only modules of its own layer or below. This prints what stands against that:

- each requirement that goes up, from a module to one in a layer above it;
- each loop: modules that require each other, directly or through others;
- each module that still requires the Pico SDK itself (rp2_sdk), which only platform/rp2 is to do in the end.

Third-party names (freertos, lwip, mbedtls, lvgl, rp2_sdk) and the made ones (board, version) have no layer.

Usage:
    python tools/module_graph.py              the three lists, then the verdict
    python tools/module_graph.py --modules    every module with what it requires, first
    python tools/module_graph.py --markdown   the lists as Markdown, for an ADR's progress

Ends with VERDICT PASS when nothing goes up, nothing loops and only platform/rp2 requires rp2_sdk, else VERDICT FAIL
with the counts, and exits 0 or 1. Until ADR-006's gates 6 and 7 are built it fails, and its counts are their meter.
"""

import argparse
import pathlib
import re
import sys

REPO = pathlib.Path(__file__).resolve().parent.parent
SRC = REPO / "src"
# Low to high: a module may require its own layer and those before it
LAYERS = ("boards", "platform", "common", "drivers", "services", "console", "web", "ui", "app")
NO_LAYER = {"freertos", "lwip", "mbedtls", "lvgl", "rp2_sdk", "board", "version"}
# The one module that is the Pico SDK's side of the platform
SDK_SIDE = "rp2"

CALL = re.compile(r"ot_module\s*\((.*?)\)", re.S)
KEYWORDS = ("SRCS", "INCLUDE_DIRS", "PRIV_INCLUDE_DIRS", "REQUIRES", "PRIV_REQUIRES")


def read_module(build_file):
    """
    Read what one module requires from its build file.

    Args:
        build_file: the module's CMakeLists.txt.

    Returns:
        (public, private): the names after REQUIRES and after PRIV_REQUIRES, over every ot_module() call in the
        file (a module a build may leave out has one call for each case).
    """
    text = re.sub(r"#.*", "", build_file.read_text(encoding="utf-8"))
    public, private = set(), set()
    for arguments in CALL.findall(text):
        keyword = None
        for word in arguments.split():
            if word in KEYWORDS:
                keyword = word
            elif keyword == "REQUIRES":
                public.add(word)
            elif keyword == "PRIV_REQUIRES":
                private.add(word)
    return public, private - public


def read_modules():
    """
    Find every module under src/ and what it requires.

    Returns:
        A dict of name -> (layer, public requires, private requires). A module's name is its folder's; the
        platform's interfaces (src/platform) are the module "platform".
    """
    modules = {}
    for build_file in sorted(SRC.glob("*/CMakeLists.txt")) + sorted(SRC.glob("*/*/CMakeLists.txt")):
        if "ot_module" not in build_file.read_text(encoding="utf-8"):
            continue
        folder = build_file.parent
        layer = folder.relative_to(SRC).parts[0]
        if folder.name in modules:
            raise ValueError(f"two modules are named {folder.name}")
        modules[folder.name] = (layer, *read_module(build_file))
    return modules


def find_loops(modules):
    """
    Find the groups of modules that require each other (Tarjan's strongly connected components).

    Args:
        modules: as read_modules() gives.

    Returns:
        A list of groups, each a sorted list of two or more module names, the largest first.
    """
    edges = {name: sorted((public | private) & set(modules)) for name, (_, public, private) in modules.items()}
    index, low, on_stack, stack, groups = {}, {}, set(), [], []

    def visit(node):
        index[node] = low[node] = len(index)
        stack.append(node)
        on_stack.add(node)
        for other in edges[node]:
            if other not in index:
                visit(other)
                low[node] = min(low[node], low[other])
            elif other in on_stack:
                low[node] = min(low[node], index[other])
        if low[node] == index[node]:
            group = []
            while True:
                member = stack.pop()
                on_stack.discard(member)
                group.append(member)
                if member == node:
                    break
            if len(group) > 1:
                groups.append(sorted(group))

    for name in sorted(modules):
        if name not in index:
            visit(name)
    return sorted(groups, key=lambda group: (-len(group), group))


def main():
    """
    Print the modules' requirements that stand against the layers, and the verdict.

    Returns:
        0 when nothing goes up, nothing loops and only the SDK's side requires rp2_sdk, else 1.
    """
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--modules", action="store_true", help="list every module with what it requires")
    parser.add_argument("--markdown", action="store_true", help="print the lists as Markdown")
    args = parser.parse_args()

    modules = read_modules()
    rank = {layer: at for at, layer in enumerate(LAYERS)}
    unknown = sorted({required for _, public, private in modules.values() for required in public | private}
                     - set(modules) - NO_LAYER)
    upward = []
    for name, (layer, public, private) in sorted(modules.items(), key=lambda item: (-rank[item[1][0]], item[0])):
        for required in sorted((public | private) & set(modules)):
            if rank[modules[required][0]] > rank[layer]:
                kind = "its headers" if required in public else "its sources"
                upward.append((f"{layer}/{name}", f"{modules[required][0]}/{required}", kind))
    loops = find_loops(modules)
    on_sdk = sorted(f"{layer}/{name}" for name, (layer, public, private) in modules.items()
                    if "rp2_sdk" in public | private and name != SDK_SIDE)

    bullet = "- " if args.markdown else "  "
    if args.modules:
        print(f"{len(modules)} modules:")
        for name, (layer, public, private) in sorted(modules.items(), key=lambda item: (-rank[item[1][0]], item[0])):
            print(f"{bullet}{layer}/{name}: requires {' '.join(sorted(public)) or 'nothing'}"
                  f"{'; its sources also ' + ' '.join(sorted(private)) if private else ''}")
    print(f"{len(upward)} requirements go up, against the layers:")
    for module, required, kind in upward:
        print(f"{bullet}{module} requires {required} ({kind})")
    print(f"{len(loops)} loops:")
    for group in loops:
        print(f"{bullet}{len(group)} modules: {' '.join(group)}")
    print(f"{len(on_sdk)} modules besides platform/{SDK_SIDE} require the Pico SDK itself:")
    if on_sdk:
        print(f"{bullet}{' '.join(on_sdk)}")
    if unknown:
        print(f"required and not found: {' '.join(unknown)}")

    if upward or loops or on_sdk or unknown:
        looped = sum(len(group) for group in loops)
        print(f"VERDICT FAIL {len(upward)} requirements go up, {looped} modules in {len(loops)} loops, "
              f"{len(on_sdk)} modules on the Pico SDK" + (f", {len(unknown)} names not found" if unknown else ""))
        return 1
    print("VERDICT PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
