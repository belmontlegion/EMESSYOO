# Release Preparation Checklist

This guide captures the steps required to publish EMESSYOO to a fresh GitHub repository (e.g., `github.com/<org>/EMESSYOO`) and to produce redistributable source archives.

## 1. Verify Third-Party Compliance
- Confirm `LICENSE` (MIT) and `THIRD_PARTY_NOTICES.md` remain accurate.
- Ensure the `JUCE` directory is **not** committed. Contributors should run `setup.bat` or `setup.sh` to fetch JUCE locally.
- If distributing binaries that statically link JUCE, ensure you satisfy the JUCE licence (AGPLv3 or commercial) before publishing.

## 2. Clean the Working Tree
Run from the repository root:

```pwsh
Get-ChildItem -Path build*,build-*,JUCE,release -ErrorAction SilentlyContinue | Remove-Item -Recurse -Force
```

Re-run `setup.(bat|sh)` afterwards when you need JUCE again. The GitHub repository should only contain source, scripts, and documentation.

## 3. Initialize the GitHub Repository

```pwsh
git init
git remote add origin git@github.com:<owner>/EMESSYOO.git
git add .
git commit -m "Initial import"
git push -u origin main
```

## 4. Build Verification (Recommended)

```pwsh
cmake -S . -B build -G "Ninja"
cmake --build build --config Release
```

Running both Debug and Release builds on Windows/Linux/macOS before tagging a release is encouraged.

## 5. Create Source Archive
Use the packaging script committed in `scripts/package-source.ps1`:

```pwsh
pwsh scripts\package-source.ps1
```

## 6. Release Checklist
- [ ] CHANGELOG / release notes written.
- [ ] `USER_GUIDE.md`, `README.md`, and `QUICKSTART.md` updated.
- [ ] Source archive uploaded to the GitHub Release page.
- [ ] Binary builds (if any) include JUCE compliance notices.

---

_Last updated: 2025-11-16_
