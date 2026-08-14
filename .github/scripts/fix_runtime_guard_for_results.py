from pathlib import Path

path = Path('patches/apply_runtime_integration.py')
text = path.read_text()
old = '''replace("src/classic.cpp", "    if(frames == 0 and level_index() < 30) {\\n", "    if(frames == 0 and (custom_levels::active() or level_index() < 30)) {\\n")'''
new = '''classic = (ROOT / "src/classic.cpp").read_text()
if "    if(frames == 0 and !custom_results and (custom_levels::active() or level_index() < 30)) {\\n" not in classic:
    replace("src/classic.cpp", "    if(frames == 0 and level_index() < 30) {\\n", "    if(frames == 0 and (custom_levels::active() or level_index() < 30)) {\\n")'''
if old not in text:
    raise SystemExit('Timer integration guard line not found')
path.write_text(text.replace(old, new, 1))
print('Updated runtime integration verifier for custom results timer guard')
