# aris-ui — agent & developer task runner.
# Every target is one command so agents (and humans) never guess build steps.
#
#   make configure  — first-time CMake configure
#   make build      — compile everything
#   make test       — build + run the unit test suite (ctest)
#   make preview    — render all UI scenes to previews/*.png (headless)
#   make demo       — build + run the interactive desktop runner
#   make docs       - regenerate docs/*.html from docs-src/*.md
#   make spv        - regenerate src/renderer/vk_spv.inc from the Vulkan GLSL
#   make verify     - build + test + preview (the full agent verification loop)
#   make clean      - remove build artifacts (previews/atlas are kept)

BUILD ?= build
JOBS  ?= $(shell nproc 2>/dev/null || echo 4)
# Override on systems where the interpreter is `python` (e.g. Windows).
PYTHON ?= python3

.PHONY: all configure build test preview demo docs spv verify clean help

all: build

help:
	@echo "targets: configure build test preview demo docs spv verify clean"

configure:
	cmake -S . -B $(BUILD) -DARIS_UI_BUILD_TESTS=ON

$(BUILD)/CMakeCache.txt: configure

build: $(BUILD)/CMakeCache.txt
	cmake --build $(BUILD) -j$(JOBS)

test: build
	ctest --test-dir $(BUILD) --output-on-failure

preview: build
	./$(BUILD)/aris_scene_preview --all --out previews

anim: build
	./$(BUILD)/aris_scene_preview --anim animation --gif --frames 60 --fps 60 --out previews

demo: build
	./$(BUILD)/aris_runner

docs:
	$(PYTHON) tools/gen_docs.py

# The Vulkan backend links against checked-in SPIR-V (src/renderer/vk_spv.inc).
# Regenerate it after editing src/renderer/shaders/ui.vert|ui.frag — or the GLSL
# copy in src/renderer/batcher.cpp. Needs glslang (tools/glslang/bin) + Python.
# `make spv CHECK=1` verifies the checked-in file without writing it.
spv:
	$(PYTHON) tools/gen_spv.py $(if $(CHECK),--check,)

# One-shot verification loop for agents: compile, test, and produce visual
# previews. Run this after any change; diff previews/*.png to confirm.
verify: test preview
	@echo "verify OK — build, tests and previews all up to date"

clean:
	cmake --build $(BUILD) --target clean 2>/dev/null || rm -rf $(BUILD)