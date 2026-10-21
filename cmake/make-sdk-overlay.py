"""Expose Microsoft's case-insensitive header tree to Clang on Linux."""
import json
import pathlib
import sys
sdk=pathlib.Path(sys.argv[1]).resolve()
roots=[]
for include in (sdk/'crt/include',sdk/'sdk/include'):
    seen=set()
    for p in sorted(include.rglob('*')):
        if not p.is_file(): continue
        key=str(p).lower()
        if key in seen: continue
        seen.add(key)
        roots.append({'type':'file','name':str(p),'external-contents':str(p.resolve())})
pathlib.Path(sys.argv[2]).write_text(json.dumps({'version':0,'case-sensitive':False,'roots':roots}))

# lld-link does not apply the Clang VFS. Retain filename stem casing while
# normalising extensions (e.g. SDK DbgHelp.Lib -> JUCE DbgHelp.lib).
for p in sdk.rglob('*'):
    if p.is_file() and p.suffix.lower()=='.lib':
        alias=p.with_suffix('.lib')
        if not alias.exists(): alias.symlink_to(p.name)

if len(sys.argv)>3:
    import re
    libraries={p.name.lower():p for p in sdk.rglob('*.lib') if p.is_file()}
    for source in pathlib.Path(sys.argv[3]).rglob('*'):
        if source.suffix.lower() not in ('.h','.cpp','.c','.mm'): continue
        for name in re.findall(r'pragma\s+comment\s*\(\s*lib\s*,\s*"([^"]+)"',source.read_text(errors='ignore')):
            original=libraries.get(name.lower())
            if original:
                alias=original.with_name(name)
                if not alias.exists(): alias.symlink_to(original.name)
