# aris-ui — agent & developer task runner.
# Every target is one command so agents (and humans) never guess build steps.
#
#   make configure  — first-time CMake configure
#   make build      — compile everything
#   make test       — build + run the unit test suite (ctest)
#   make preview    — render all UI scenes to previews/*.png (headless)
#   make demo       — build + run the interactive desktop runner
#   make docs       — regenerate docs/*.html from docs-src/*.md
#   make verify     — build + test + preview (the full agent verification loop)
#   make clean      — remove build artifacts (previews/atlas are kept)

BUILD ?= build
JOBS  ?= $(shell nproc 2>/dev/null || echo 4)

.PHONY: all configure build test preview demo docs verify clean help

all: build

help:
	@echo "targets: configure build test preview demo docs verify clean"

configure:
	cmake -S . -B $(BUILD) -DARIS_UI_BUILD_TESTS=ON

$(BUILD)/CMakeCache.txt: configure

build: $(BUILD)/CMakeCache.txt
	cmake --build $(BUILD) -j$(JOBS)

test: build
	ctest --test-dir $(BUILD) --output-on-failure

preview: build
	./$(BUILD)/aris_scene_preview --all --out previews

demo: build
	./$(BUILD)/aris_runner

docs:
	python3 tools/gen_docs.py

# One-shot verification loop for agents: compile, test, and produce visual
# previews. Run this after any change; diff previews/*.png to confirm.
verify: test preview
	@echo "verify OK — build, tests and previews all up to date"

clean:
	cmake --build $(BUILD) --target clean 2>/dev/null || rm -rf $(BUILD)