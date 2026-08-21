# New Classic authenticated-content dependency audit

`th03-content-audit` reports every external asset known by the production
`main_resource_manifest()`. It does not maintain a second filename list. In
validated mode, every entry is opened through `MainResourceLoader`, including
the normal exact size and FNV-1a integrity policy.

Manifest-only report:

```powershell
out/build/gcc/th03-content-audit.exe > content-manifest.json
```

Validate an extracted installation and identify resources supplied by the
authenticated English overlay:

```powershell
out/build/gcc/th03-content-audit.exe `
  --data path/to/extracted-content `
  --language-overlay path/to/TH3EN.DAT `
  > content-audit.json
```

The deterministic `T3CDA001` JSON contains:

- a frozen manifest fingerprint and aggregate pass/fail counts;
- per-scene original, authenticated-override, and failed counts;
- all logical roles, archive members, decoders, exact sizes, and expected
  fingerprints; and
- semantic surfaces now owned by `T3LNG001`, `T3GTL001`, or `T3CTP001`.

Every semantic surface deliberately says `source_backing_required: true`.
Replacing glyph semantics does not make the underlying authenticated font,
background, script commands, timing, pictures, or music distributable or
unnecessary. A future asset migration should change the production resource
owner first; this report will then change through the shared manifest instead
of through an optimistic documentation edit.

The current production manifest contains 271 logical resources across 11
scene families and has FNV-1a-64 fingerprint `109855200A365C86`. The accepted
English profile validates all 271 resources and sources 52 of them from the
whole-archive-authenticated TH3EN overlay.
