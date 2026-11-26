- When work is completed, adb install release mode on the Google TV Streamer.
    If you are mid-task or checking something, use debug mode.
- Before committing, ensure GitHub Actions quality checks pass.
- Before finishing changes to C++ code, run relevant `cppcheck` checks for the modified code; do not run `cppcheck` as a pre-commit hook.
- When the user requests deployment, fix any failing relevant tests before deploying; never bypass, skip, or weaken tests to make the deployment pass.

# Release notes
- Before publishing, inspect changes since the last published release, including the diff, not just commit titles. Write concise plain-English notes about actual user-visible changes.
- Pass notes as the release_notes workflow_dispatch input (max 500 characters); listing-only refreshes do not require notes. Never use raw commit subjects, unverified claims, or generic placeholder text. Prepare notes before triggering the release workflow.
- The release workflow uses the same notes for supported release destinations. Do not publish an extra release solely because release automation changed. Do not rewrite historical releases.
