# U8G2 Project Cleanup Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Move active U8G2/U8X8 files into `Middlewares/Third_Party/u8g2`, move unused U8G2-related files into `archive/u8g2-unused`, and keep generated build artifacts out of git without changing application behavior.

**Architecture:** Treat U8G2 as a third-party dependency, not application code. Keep the current SSD1306 128x64 I2C path active and move unrelated display drivers, U8X8 widgets, MUI, and log helpers outside CubeIDE's source tree. Update CubeIDE include paths so `main.c` can keep including `u8g2.h` without logic changes.

**Tech Stack:** STM32CubeIDE managed C project, STM32F103 HAL, FatFs, U8G2/U8X8, PowerShell, git.

---

## File Structure

**Create:**
- `Middlewares/Third_Party/u8g2/include/` for active U8G2 headers.
- `Middlewares/Third_Party/u8g2/src/` for active U8G2/U8X8 source files.
- `archive/u8g2-unused/Core_Src/` for unused U8G2-related source files.
- `archive/u8g2-unused/Core_Inc/` for unused U8G2-related headers.

**Move active headers:**
- `Core/Inc/u8g2.h` -> `Middlewares/Third_Party/u8g2/include/u8g2.h`
- `Core/Inc/u8x8.h` -> `Middlewares/Third_Party/u8g2/include/u8x8.h`

**Move active sources:**
- `Core/Src/u8g2_buffer.c`
- `Core/Src/u8g2_d_memory.c`
- `Core/Src/u8g2_d_setup.c`
- `Core/Src/u8g2_font.c`
- `Core/Src/u8g2_fonts.c`
- `Core/Src/u8g2_hvline.c`
- `Core/Src/u8g2_kerning.c`
- `Core/Src/u8g2_ll_hvline.c`
- `Core/Src/u8g2_setup.c`
- `Core/Src/u8x8_byte.c`
- `Core/Src/u8x8_cad.c`
- `Core/Src/u8x8_display.c`
- `Core/Src/u8x8_d_ssd1306_128x64_noname.c`
- `Core/Src/u8x8_gpio.c`
- `Core/Src/u8x8_setup.c`

**Move unused U8G2-related files to archive:**
- Remaining `Core/Src/u8g2_*.c`
- Remaining `Core/Src/u8x8_*.c`
- `Core/Src/u8log.c`
- `Core/Src/u8log_u8g2.c`
- `Core/Src/u8log_u8x8.c`
- `Core/Src/mui.c`
- `Core/Src/mui_u8g2.c`
- `Core/Inc/mui.h`
- `Core/Inc/mui_u8g2.h`

**Modify:**
- `.cproject`: add `../Middlewares/Third_Party/u8g2/include` to both Debug and Release C compiler include paths.
- `.gitignore`: keep `Debug/`, `Release/`, `.metadata/`, `*.o`, `*.su`, `*.cyclo`, and `*.d`; add `*.elf`, `*.map`, `*.list`, `*.bin`, and `*.hex`.

**Git index cleanup:**
- Remove tracked `Debug/` and `Release/` generated artifacts from git with `git rm -r --cached Debug Release`; do not delete local build files from disk.

---

### Task 1: Snapshot Current State

**Files:**
- Read: `Core/Src/main.c`
- Read: `.cproject`
- Read: `.gitignore`
- Read: `Core/Src/u8g2_*.c`, `Core/Src/u8x8_*.c`, `Core/Inc/u8g2.h`, `Core/Inc/u8x8.h`

- [ ] **Step 1: Confirm the active OLED setup symbol**

Run:

```powershell
rg -n "u8g2_Setup_ssd1306_i2c_128x64_noname_f|u8g2_font_ncenB10_tr|u8g2_font_ncenB08_tr|u8g2_font_5x7_tr" Core\Src\main.c Core\Inc\u8g2.h Core\Src\u8g2_fonts.c
```

Expected:

```text
Core\Src\main.c contains u8g2_Setup_ssd1306_i2c_128x64_noname_f
Core\Src\main.c references u8g2_font_ncenB10_tr, u8g2_font_ncenB08_tr, and u8g2_font_5x7_tr
Core\Inc\u8g2.h declares those font symbols
Core\Src\u8g2_fonts.c defines those font symbols
```

- [ ] **Step 2: Confirm the active U8G2/U8X8 file counts before moving**

Run:

```powershell
Get-ChildItem Core\Src -Filter 'u8g2_*.c' | Measure-Object
Get-ChildItem Core\Src -Filter 'u8x8_*.c' | Measure-Object
Get-ChildItem Core\Src -Filter 'u8log*.c' | Measure-Object
Get-ChildItem Core\Src -Filter 'mui*.c' | Measure-Object
Get-ChildItem Core\Inc -Filter 'u8*.h' | Measure-Object
Get-ChildItem Core\Inc -Filter 'mui*.h' | Measure-Object
```

Expected:

```text
u8g2 source count is 21
u8x8 source count is 103
u8log source count is 3
mui source count is 2
u8 header count is 2
mui header count is 2
```

- [ ] **Step 3: Confirm only the plan and previously approved design are unrelated to cleanup state**

Run:

```powershell
git status --short
```

Expected:

```text
Existing user edits may appear in Core/Src/main.c, Core/Src/sd_logger.c, and generated Debug outputs.
Do not revert those edits.
```

---

### Task 2: Create Destination Folders

**Files:**
- Create: `Middlewares/Third_Party/u8g2/include/`
- Create: `Middlewares/Third_Party/u8g2/src/`
- Create: `archive/u8g2-unused/Core_Src/`
- Create: `archive/u8g2-unused/Core_Inc/`

- [ ] **Step 1: Create the folder structure**

Run:

```powershell
New-Item -ItemType Directory -Force -Path Middlewares\Third_Party\u8g2\include | Out-Null
New-Item -ItemType Directory -Force -Path Middlewares\Third_Party\u8g2\src | Out-Null
New-Item -ItemType Directory -Force -Path archive\u8g2-unused\Core_Src | Out-Null
New-Item -ItemType Directory -Force -Path archive\u8g2-unused\Core_Inc | Out-Null
```

Expected:

```text
All four directories exist.
No source files have moved yet.
```

- [ ] **Step 2: Verify destination paths are inside the repository**

Run:

```powershell
Resolve-Path Middlewares\Third_Party\u8g2\include
Resolve-Path Middlewares\Third_Party\u8g2\src
Resolve-Path archive\u8g2-unused\Core_Src
Resolve-Path archive\u8g2-unused\Core_Inc
```

Expected:

```text
Every resolved path starts with C:\Users\User\Documents\GitHub\PMS7003_OLED_I2C
```

---

### Task 3: Move Active U8G2 Files

**Files:**
- Move: active headers and active source files listed in File Structure.

- [ ] **Step 1: Move active headers**

Run:

```powershell
Move-Item -LiteralPath Core\Inc\u8g2.h -Destination Middlewares\Third_Party\u8g2\include\u8g2.h
Move-Item -LiteralPath Core\Inc\u8x8.h -Destination Middlewares\Third_Party\u8g2\include\u8x8.h
```

Expected:

```text
Middlewares\Third_Party\u8g2\include\u8g2.h exists
Middlewares\Third_Party\u8g2\include\u8x8.h exists
Core\Inc\u8g2.h no longer exists
Core\Inc\u8x8.h no longer exists
```

- [ ] **Step 2: Move active sources**

Run:

```powershell
$activeSources = @(
  'u8g2_buffer.c',
  'u8g2_d_memory.c',
  'u8g2_d_setup.c',
  'u8g2_font.c',
  'u8g2_fonts.c',
  'u8g2_hvline.c',
  'u8g2_kerning.c',
  'u8g2_ll_hvline.c',
  'u8g2_setup.c',
  'u8x8_byte.c',
  'u8x8_cad.c',
  'u8x8_display.c',
  'u8x8_d_ssd1306_128x64_noname.c',
  'u8x8_gpio.c',
  'u8x8_setup.c'
)
foreach ($name in $activeSources) {
  Move-Item -LiteralPath (Join-Path 'Core\Src' $name) -Destination (Join-Path 'Middlewares\Third_Party\u8g2\src' $name)
}
```

Expected:

```text
The 15 active source files exist under Middlewares\Third_Party\u8g2\src.
The same 15 files no longer exist under Core\Src.
```

- [ ] **Step 3: Verify active files landed in the new source tree**

Run:

```powershell
Get-ChildItem Middlewares\Third_Party\u8g2\include | Select-Object -ExpandProperty Name | Sort-Object
Get-ChildItem Middlewares\Third_Party\u8g2\src | Select-Object -ExpandProperty Name | Sort-Object
```

Expected:

```text
include contains u8g2.h and u8x8.h.
src contains the 15 active source files from Step 2.
```

---

### Task 4: Move Unused U8G2-Related Files To Archive

**Files:**
- Move: remaining `Core/Src/u8g2_*.c`
- Move: remaining `Core/Src/u8x8_*.c`
- Move: `Core/Src/u8log*.c`
- Move: `Core/Src/mui*.c`
- Move: `Core/Inc/mui*.h`

- [ ] **Step 1: Move remaining U8G2/U8X8 sources**

Run:

```powershell
Get-ChildItem Core\Src -Filter 'u8g2_*.c' | Move-Item -Destination archive\u8g2-unused\Core_Src
Get-ChildItem Core\Src -Filter 'u8x8_*.c' | Move-Item -Destination archive\u8g2-unused\Core_Src
```

Expected:

```text
Core\Src has no files matching u8g2_*.c.
Core\Src has no files matching u8x8_*.c.
archive\u8g2-unused\Core_Src contains the unused U8G2/U8X8 source files.
```

- [ ] **Step 2: Move U8G2-related log and MUI files**

Run:

```powershell
Get-ChildItem Core\Src -Filter 'u8log*.c' | Move-Item -Destination archive\u8g2-unused\Core_Src
Get-ChildItem Core\Src -Filter 'mui*.c' | Move-Item -Destination archive\u8g2-unused\Core_Src
Get-ChildItem Core\Inc -Filter 'mui*.h' | Move-Item -Destination archive\u8g2-unused\Core_Inc
```

Expected:

```text
Core\Src has no u8log*.c files.
Core\Src has no mui*.c files.
Core\Inc has no mui*.h files.
archive\u8g2-unused\Core_Src contains u8log.c, u8log_u8g2.c, u8log_u8x8.c, mui.c, and mui_u8g2.c.
archive\u8g2-unused\Core_Inc contains mui.h and mui_u8g2.h.
```

- [ ] **Step 3: Confirm application files remain in Core**

Run:

```powershell
Get-ChildItem Core\Src -File | Select-Object -ExpandProperty Name | Sort-Object
Get-ChildItem Core\Inc -File | Select-Object -ExpandProperty Name | Sort-Object
```

Expected:

```text
Core\Src still contains main.c and project-owned drivers such as bme280.c, ds1307.c, pms7003.c, scd41.c, sd_logger.c, gpio.c, i2c.c, spi.c, usart.c, and STM32 system files.
Core\Inc still contains matching project-owned headers.
```

---

### Task 5: Update CubeIDE Include Paths

**Files:**
- Modify: `.cproject`

- [ ] **Step 1: Add U8G2 include path to both Debug and Release compiler options**

Edit `.cproject` so each C compiler include path block contains this entry:

```xml
<listOptionValue builtIn="false" value="../Middlewares/Third_Party/u8g2/include"/>
```

Place it after the existing FatFs include entry in both include path sections:

```xml
<listOptionValue builtIn="false" value="../Middlewares/Third_Party/FatFs/src"/>
<listOptionValue builtIn="false" value="../Middlewares/Third_Party/u8g2/include"/>
```

- [ ] **Step 2: Update the CubeIDE defaults include-path string**

Edit both `com.st.stm32cube.ide.mcu.gnu.managedbuild.option.defaults` `value` attributes so the include path list contains:

```text
../Middlewares/Third_Party/FatFs/src | ../Middlewares/Third_Party/u8g2/include
```

Expected:

```text
.cproject contains four references to ../Middlewares/Third_Party/u8g2/include:
two inside defaults strings and two inside explicit include path option lists.
```

- [ ] **Step 3: Verify include path metadata**

Run:

```powershell
rg -n "Middlewares/Third_Party/u8g2/include" .cproject
```

Expected:

```text
Exactly 4 matches.
```

---

### Task 6: Update Git Ignore And Remove Tracked Build Outputs

**Files:**
- Modify: `.gitignore`
- Git index: remove tracked `Debug/` and `Release/` generated outputs.

- [ ] **Step 1: Extend generated artifact ignore rules**

Edit `.gitignore` to contain exactly this generated-output block:

```gitignore
Debug/
Release/
.metadata/
*.o
*.su
*.cyclo
*.d
*.elf
*.map
*.list
*.bin
*.hex
```

- [ ] **Step 2: Remove tracked generated artifacts from git without deleting local files**

Run:

```powershell
git rm -r --cached Debug Release
```

Expected:

```text
Git stages deletions for tracked files under Debug and Release.
The local Debug and Release directories still exist on disk.
```

- [ ] **Step 3: Verify generated artifacts are ignored after removal from the index**

Run:

```powershell
git status --short --ignored Debug Release
```

Expected:

```text
Debug/ and Release/ appear as ignored local directories, not as modified tracked files.
```

---

### Task 7: Build And Dependency Verification

**Files:**
- Read: generated build output
- Possible Move: `archive/u8g2-unused/Core_Src/u8x8_capture.c` back to `Middlewares/Third_Party/u8g2/src/u8x8_capture.c` if the linker reports unresolved `u8x8_capture_*` symbols.

- [ ] **Step 1: Locate the build command**

Run:

```powershell
Get-Command make -ErrorAction SilentlyContinue
Get-Command arm-none-eabi-gcc -ErrorAction SilentlyContinue
```

Expected:

```text
If both commands are found, run the clean build in Step 2.
If either command is missing, skip to Step 4 and report that local CLI build verification is unavailable.
```

- [ ] **Step 2: Run a clean Debug build when the toolchain is available**

Run:

```powershell
Set-Location Debug
make clean
make -j8 all
Set-Location ..
```

Expected:

```text
Build reaches the linker.
No fatal include errors for u8g2.h or u8x8.h.
No undefined references to U8G2/U8X8 symbols.
```

- [ ] **Step 3: Restore `u8x8_capture.c` only if the linker needs it**

If Step 2 reports undefined references named `u8x8_capture_*`, run:

```powershell
Move-Item -LiteralPath archive\u8g2-unused\Core_Src\u8x8_capture.c -Destination Middlewares\Third_Party\u8g2\src\u8x8_capture.c
Set-Location Debug
make -j8 all
Set-Location ..
```

Expected:

```text
The rebuild succeeds or advances to a different concrete missing symbol.
Do not move other archive files back unless a build error names their symbols.
```

- [ ] **Step 4: Verify there are no active U8G2 files left in Core**

Run:

```powershell
Get-ChildItem Core\Src -Filter 'u8g2_*.c'
Get-ChildItem Core\Src -Filter 'u8x8_*.c'
Get-ChildItem Core\Inc -Filter 'u8*.h'
```

Expected:

```text
No output for Core\Src U8G2/U8X8 sources.
No output for Core\Inc u8*.h headers.
```

---

### Task 8: Review And Commit Cleanup

**Files:**
- Review: all moved files, `.cproject`, `.gitignore`, git index state.

- [ ] **Step 1: Review changed file categories**

Run:

```powershell
git status --short
```

Expected:

```text
Moved U8G2 active files show under Middlewares/Third_Party/u8g2.
Moved unused U8G2-related files show under archive/u8g2-unused.
.cproject and .gitignore are modified.
Debug and Release tracked artifacts are staged for removal from git.
Existing user edits in Core/Src/main.c and Core/Src/sd_logger.c are still present and not reverted.
```

- [ ] **Step 2: Review metadata edits**

Run:

```powershell
git diff -- .cproject .gitignore
```

Expected:

```text
.cproject only adds ../Middlewares/Third_Party/u8g2/include to Debug and Release include paths/defaults.
.gitignore only adds generated output extensions if they were missing.
```

- [ ] **Step 3: Stage intended cleanup files**

Run:

```powershell
git add -A -- .cproject .gitignore Middlewares\Third_Party\u8g2 archive\u8g2-unused Debug Release 'Core/Src/u8g2_*.c' 'Core/Src/u8x8_*.c' 'Core/Src/u8log*.c' 'Core/Src/mui*.c' 'Core/Inc/u8*.h' 'Core/Inc/mui*.h'
```

Expected:

```text
Only intended U8G2 moves, archive moves, generated artifact removals, and metadata edits are staged.
Existing application logic edits such as Core/Src/main.c and Core/Src/sd_logger.c are not staged by this command.
```

- [ ] **Step 4: Commit the cleanup**

Run:

```powershell
git commit -m "chore: organize u8g2 sources"
```

Expected:

```text
A commit is created for U8G2 project cleanup.
```

- [ ] **Step 5: Report verification**

Run:

```powershell
git status --short
```

Expected:

```text
Only pre-existing unrelated source edits remain, if any.
Debug/ and Release/ no longer appear as tracked modifications.
```
