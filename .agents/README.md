# Embedded AI Harness — vendored skills

These are the skills of the [Embedded AI Harness](https://github.com/sprchuoi/Embedded-AI-Harness),
committed here as **real files** rather than symlinked, so the harness travels
with this repository and a fresh clone is immediately usable.

`.claude/skills` is a symlink to this directory. That is deliberate and load
bearing: several harness skills shell out to `.claude/skills/...` paths (for
example `esp-idf-handling/discover-testbench.py`), so that path must resolve
even when the harness is driven from DeepSeek Harness, which reads
`.agents/skills` and does not scan `.claude/`.

## Layout

```
.agents/skills/     the 18 skills (this directory)
.agents/README.md   this file — not a skill, outside the discovery root
.claude/skills      symlink to ../.agents/skills
```

Discovery root is `.agents/skills`; files here that are not skill bundles are
ignored.

## Updating from upstream

```bash
cp -r ~/sandboxes/Embedded-AI-Harness/.claude/skills/. \
      ~/sandboxes/smart_home_idf/.agents/skills/
```

Re-apply the frontmatter fix below afterwards — upstream still has the bug.

## Local divergence from upstream

One deliberate change, tracked as `~/sandboxes/eah-yaml-frontmatter-fix.patch`:

`setup-action` and `testbench-install` shipped frontmatter that is **invalid
YAML**. Their `description:` was a single-line plain scalar containing `": "`,
which YAML parses as a nested mapping, so any strict parser rejects the whole
file and the skill vanishes. Two skills were silently invisible.

Both now use a folded block scalar (`description: >-`). The description text is
byte-identical; only the YAML form changed. `skill-filesystem` in DeepSeek
Harness observes this and drops the skill with a warning, which is how it was
caught.

## Verifying integrity

```bash
# should print nothing if this tree still matches the canonical clone
diff -rq ~/sandboxes/Embedded-AI-Harness/.claude/skills .agents/skills

# every skill's frontmatter must parse
python3 - <<'EOF'
import yaml, glob
for p in sorted(glob.glob('.agents/skills/*/SKILL.md')):
    raw = open(p, encoding='utf-8').read()
    yaml.safe_load(raw[3:raw.index('\n---', 3)])
print('all frontmatter parses')
EOF
```
