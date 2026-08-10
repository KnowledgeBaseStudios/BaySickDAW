# Building BaySickDAW

This page is for someone who has never built a C++ program before. Follow it top
to bottom and you will end up with a working `BaySickDAW.exe`.

BaySickDAW is a Windows desktop application. There is no Mac or Linux build.

---

## 1. What you need to install first

### Visual Studio (the free one)

1. Go to https://visualstudio.microsoft.com/downloads/
2. Download **Visual Studio Community**. It is free. Run the installer.
3. The installer shows a page called **Workloads**. Tick the box for
   **Desktop development with C++**. This is the important step. Without it you
   have an editor but no compiler, and the build cannot start.
4. Click Install and let it finish. It is a big download, several GB.

Any edition works (Community, Professional, Enterprise) and any version from
2017 onward. The build script finds whichever one you have on its own.

### CMake

CMake is the tool that turns this project into something Visual Studio can
build. You have two options, either is fine:

- **Option A:** download it from https://cmake.org/download/ - pick the
  **Windows x64 Installer**. On the options page during install, choose
  **Add CMake to the system PATH for all users**.
- **Option B:** in the Visual Studio installer, go to the
  **Individual components** tab and tick **C++ CMake tools for Windows**.

If you already have CMake, make sure it is a recent version. Version 3.22 is the
absolute minimum, but a CMake older than your Visual Studio will refuse to set
the project up, so newest is safest.

### The code

Download the whole repository, not just some folders. This project carries its
own copies of the libraries it depends on (JUCE, sfizz, RubberBand and others),
so a partial download will fail at setup time.

If you use Git:

```
git clone https://github.com/KnowledgeBaseStudios/BaySickDAW.git
```

You can put the folder anywhere. A path with spaces in it is fine.

---

## 2. Build it

Open a **Command Prompt** window, change into the folder you downloaded, and
run:

```
do_build.bat
```

That is the whole thing. One command, no arguments, no configuration.

Double clicking `do_build.bat` in Explorer builds exactly the same way, with one
catch: the window closes the instant the build ends, so the summary is gone
before you can read it. Either of these gets it back:

- **Read `build_log.txt` afterwards.** It lands in the same folder as
  `do_build.bat` and contains everything the window showed, plus the full
  compiler output. Nothing is lost by missing the window.
- **Or build from a shortcut that waits for you.** Right click `do_build.bat`
  and choose **Create shortcut** - on Windows 11 it is under **Show more
  options**. Right click the new shortcut, choose **Properties**, and in the
  **Target** box add a space and the word `pause` at the very end, after the
  closing quote mark. Click OK. Double clicking that shortcut runs the build and
  then waits for you to press a key, so the summary stays on screen.

### What to expect

- The **first** build sets the project up and then compiles everything from
  scratch. Budget a long time, potentially an hour on a slower machine, and
  about **9 GB of free disk space** for the build folders.
- Later builds only recompile what changed and are far quicker.
- The script prints what it is doing as it goes and finishes with a summary
  block. Everything it prints, plus the full compiler output, is also saved to
  **`build_log.txt`** in the same folder.
- `build_log.txt` is emptied and started fresh at the beginning of every run,
  including a run that stops early because something is missing. It always
  describes the build you just ran, never an older one.

A successful run ends like this:

```
==================== BUILD SUMMARY ====================
  Release app                : exit 0
  Debug app                  : exit 0
  Plugin host 64-bit         : exit 0
  Plugin host 32-bit setup   : exit 0
  Plugin host 32-bit build   : exit 0
  0 means that step worked.

  ALL FIVE STEPS SUCCEEDED.
```

Five steps, five zeros. Anything other than zero means that step failed.

After those five steps the script also checks that the files they were supposed
to produce are really on disk. If one is missing it says so and reports a
failure, even when all five steps said zero - a step can report success and
still fail to put the file where the app needs it.

---

## 3. Where the app ends up

Two copies are built, from the same code:

| Which | Where |
|---|---|
| **Release** - the real app, use this one | `build\BaySickDAWStandalone_artefacts\Release\BaySickDAW.exe` |
| **Debug** - slower, pops up a dialog when it catches an internal problem | `build\BaySickDAWStandalone_artefacts\Debug\BaySickDAW.exe` |

Those paths are relative to the folder you downloaded.

Use **Release** for actually making music. Use **Debug** if you are chasing a
bug and want the app to tell you where it went wrong. The Debug window title has
`[DEBUG]` in it so you can tell them apart.

Do not run both at the same time. They fight over the audio device.

---

## 4. When it fails

**First: read the last few lines the script printed.** If something it needs is
missing, it says exactly what to install and where to get it. That covers the
majority of first-time failures.

Otherwise, open `build_log.txt` in Notepad, press Ctrl+End to jump to the bottom,
and search upward for the word `error`. The first `error` line is the real
problem, everything after it is noise.

Common causes:

| What you see | What it means |
|---|---|
| "Visual Studio was not found" | Install it, see section 1. |
| "the C++ part is not" installed | You have Visual Studio but skipped the **Desktop development with C++** workload. Open **Visual Studio Installer** from the Start menu, click **Modify**, tick it, click Modify again. |
| "CMake was not found" | Install CMake, see section 1. |
| "the first-time project setup step failed" | Usually an incomplete download of the code, or a CMake older than your Visual Studio. |
| `error LNK1168: cannot open ... BaySickDAW.exe for writing` | The app is still running. Close BaySickDAW and build again. |
| Something about a `.pdb` being locked | Same thing. Close the app, and close Visual Studio if you had it open on this project. |
| Errors mentioning a path that is not where the code currently lives | You moved or renamed the folder after building once. The `build\` and `build32\` folders remember the old location. Delete both and run `do_build.bat` again. |
| "build_log.txt cannot be replaced" | Another program is holding that file open, or the folder is read-only. The build refuses to start rather than leave you reading the previous run's log. Close whatever has it open and run again. |
| "A file that should have been built or copied is not there" | Something was still running and holding one of the built files. Close BaySickDAW, and any plugin window it had open, then build again. |

If the compiler itself errors out on a source file, that is a bug in the code
rather than a problem with your machine. Copy the first `error` line out of
`build_log.txt` and send it over.

---

## 5. Making a tester installer

This packages the build you just made into a single setup file, so a tester can
be handed one file instead of a build tree.

**This is not the release installer.** It carries no manual, no license or EULA
page, no legal review of the third party notices, and no code signature. The
real one is owned by a later batch of work that has not run yet. Do not put what
this produces on a download page.

### Build it

You need **NSIS** once, from https://nsis.sourceforge.io/Download. Accept the
default install folder. Nothing else needs configuring.

Then, from the folder you downloaded, in this order:

```
do_build.bat
make_installer.bat
```

`make_installer.bat` never compiles anything. It only packages what
`do_build.bat` already produced, and it stops with a plain English message if
any piece is missing rather than making a setup file that would fail on a
tester's PC. Its full output goes to `installer_log.txt` next to it, the same
way `do_build.bat` writes `build_log.txt`.

The finished file lands here, with the version taken straight from
`CMakeLists.txt` so it can never claim a version the app does not have:

```
Installer\BaySickDAW-<version>-Tester-Setup.exe
```

It is around 19 MB. Building it takes a minute or two, almost all of it
compression.

### What is in it, and what is not

| In the package | Where it installs |
|---|---|
| `BaySickDAW.exe` | the install folder |
| `BaySickPluginHost64.exe`, `BaySickPluginHost32.exe` | the install folder - without these, bridged and 32-bit plugins do not load at all |
| `Resources\` | the install folder - cassette IRs, hiss beds and acoustic IRs that the effects load at run time |
| Factory `Presets\` | `Documents\BaySickDAW\Presets` |
| Factory `Templates\` | `Documents\BaySickDAW\Templates` |

**The sound library is not in it.** That is about 4 GB, and the app fetches it
itself the first time it starts, and from **Options > Get Sound Content** at any
time after that.

The package contains factory content only. Every `My Presets` and
`My Templates` folder is excluded when it is built, so it physically cannot
contain, and therefore cannot overwrite, anything a tester saved themselves.

### What a tester does with it

1. **Windows will block it.** It is not code signed, so a blue box appears
   saying **"Windows protected your PC"**. Click **More info**, then **Run
   anyway**. That warning means the file has no signature yet, not that anything
   is wrong with it. Tell every tester this up front - most people cancel at
   that screen.
2. It installs **for the current user only**. No administrator password, nothing
   in Program Files. The default location is
   `%LOCALAPPDATA%\Programs\BaySickDAW`.
3. A Start Menu shortcut is always created. A desktop shortcut is an optional
   tick box on the components page.
4. On **first launch** the app offers to download the sound library. Until that
   finishes, the sampled instruments - drums, guitars, basses, keys, strings,
   brass and woodwinds - have nothing to play and are silent. Everything else
   works normally. The download can be stopped and picked up later.

### Uninstalling

There is an entry in **Add or remove programs**, plus a shortcut in the Start
Menu folder.

Uninstalling removes the program files and the shortcuts, and **nothing else**.
It does not touch `Documents\BaySickDAW`, so projects, presets, templates and
the downloaded sound library all survive. Reinstalling therefore does not cost
another 4 GB download.

---

## 6. Notes for the curious

- The script builds five things: the app in Release, the app in Debug, and three
  pieces of the plugin sandbox helper that lets BaySickDAW load third party VST3
  plugins without a crashing plugin taking the whole app down. The 32-bit helper
  needs its own separate setup step, which is why the count is five and not two.
- Nothing about your machine needs to be configured by hand. The script works
  out where the repository is from its own location, finds Visual Studio through
  the standard `vswhere` tool that ships with it, and finds CMake on your PATH
  or in the usual install locations.
- `build\` and `build32\` are generated. Deleting them costs you nothing except
  the time of a full rebuild.
- ASIO audio driver support is compiled in automatically. The needed headers are
  already in the repository under `libs\asiosdk`, so there is nothing to fetch.
