- When work is completed, adb install release mode on the Google TV Streamer.
    If you are mid-task or checking something, use debug mode.
- Before committing, ensure GitHub Actions quality checks pass.
- Before finishing changes to C++ code, run relevant `cppcheck` checks for the modified code; do not run `cppcheck` as a pre-commit hook.
- When the user requests deployment, fix any failing relevant tests before deploying; never bypass, skip, or weaken tests to make the deployment pass.
