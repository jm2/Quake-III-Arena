# Running the port on Mac OS 9

How to install a build on Mac OS 9, test it one subsystem at a time, and bring
back what is needed to fix what breaks. This is written for real hardware with a
3D accelerator. [ClassicMac](#on-classicmac) runs nearly all of it too, and is
the quicker first step.

## What to copy

Build with `./build_mac.sh --base-only` and take `build_mac/Quake3.bin` to the
Mac. It is MacBinary: StuffIt Expander decodes it. Keep `build_mac/Quake3.xcoff`
on the build machine. It turns crash addresses into source lines, and only the
XCOFF from the same build will do.

Do not use the old contents of `release_mac/`: they are the failed April run,
with demo data. Do not run from `Quake3.dsk` or a locked image: there is no
`baseq3` beside the application there, and a locked disk cannot hold the logs.

## Install

Everything goes in one folder on a writable disk. The engine looks for `baseq3`
beside the application, and writes its config and logs there:

```text
Quake III Arena:
    Quake3
    MacQuake3Parms.txt      (from the stages below)
    baseq3:
        pak0.pk3 ... pak8.pk3
        bringup.cfg         (below)
```

- `pak0.pk3` must be the retail one, from the CD. `pak1.pk3` to `pak8.pk3` are
  the point releases, and 1.32 needs them all. The demo's `pak0.pk3` stops at
  `Couldn't load default.cfg`, because it has no `productid.txt`.
- Give the application at least 120 MB in Get Info. It needs a Mac with enough
  RAM for that, or virtual memory on.
- Use Thousands of colors for windowed stages. On 256 colors, windowed mode can
  stop with "at least four megs of video memory", because no accelerated
  renderer is reported on that desktop.
- Launching needs `InputSprocketLib`, `DrawSprocketLib` and `OpenGLLibrary` in
  the Extensions folder, and Open Transport. A missing library stops the launch
  with a message naming it.
- MacsBug is worth installing. See [Crashes](#crashes).

## Logs

| File, beside the application | Written | Survives a freeze |
| --- | --- | --- |
| `q3boot.log` | line by line | yes |
| `q3boot.prev.log` | the previous launch's `q3boot.log` | yes |
| `flight_record_init.txt` | at the end of `Com_Init` | if it got there |
| `crashdump.txt`, `retro68_console_crash.txt` | at a fatal error | no |
| `retro68_console.txt` | at quit, and on Cmd-D | no |

`q3boot.log` is the one that matters. It has a fixed length of 2 MB until a
clean quit, so its size says nothing, and the end of the text is marked
`<<< end of log >>>`. Each line starts with seconds since launch. It starts with
the revision, the machine and the memory partition, and a `heartbeat:` line every
5 seconds gives a rough frame rate. Heartbeats come between frames, and a map
load happens inside one frame, so there are none during a load. If the last line
is a heartbeat, the game stopped somewhere in the next few seconds without
printing anything. Otherwise, what was printed last says how far it got.

**Restarting after a freeze moves `q3boot.log` to `q3boot.prev.log`, and a
second restart loses it.** Copy it somewhere before the next launch. Close it
first: with `q3boot.prev.log` open in another application, a launch cannot start
a new log, so `q3boot.log` stays as an older run's. The time in the Finder shows
which run a log is from.

Every line is a disk write, which slows startup and map loads a little, and a
warning printed every frame would slow the game. `+set sys_bootlog 0` turns the
log off, if it is ever in doubt.

## Stages

Each stage adds one subsystem that has never run on this build, so a failure
says where to look. Put the lines in `MacQuake3Parms.txt` and launch. Holding
Shift at launch types them instead, for one launch, and ignores the file.

Commands typed at the console in a game need a leading `/`: as in retail, plain
text is chat. To reset between stages, delete `baseq3:q3config.cfg`, or add `safe` as the first
line, which ignores it. Settings are saved there, so a setting from one stage
otherwise follows you into the next. A line that does not start with `set`, such
as `+map`, also skips the opening cinematic, as on other platforms. The file holds
about 16 `+set` lines, and a command past that limit does not take effect.

### 1. Window, no network, no sound

```text
+set r_fullscreen 0
+set net_noudp 1
+exec bringup.cfg
```

`bringup.cfg`, in `baseq3`, runs a bot match by itself. It covers three maps, a
`vid_restart`, a disconnect and a quit:

```text
set bot_enable 1
set sv_maxclients 8
map q3dm1
wait 300
addbot sarge 2
addbot grunt 2
wait 1500
echo ==== stage: q3dm17
map q3dm17
wait 300
addbot sarge 3
wait 1500
echo ==== stage: vid_restart
vid_restart
wait 300
echo ==== stage: q3tourney2
map q3tourney2
wait 600
echo ==== stage: disconnect
disconnect
wait 300
echo ==== stage: quit
quit
```

Expect `Loading dll file` and never `interpreting it` for stock paks: that line
means a pak's checksum is not a retail one, and the game will run far slower.
Expect a clean quit, and `q3boot.log` then shrinks to its text.

### 2. Fullscreen

The same, without the `r_fullscreen` line. This is DrawSprocket. It should
return to the desktop with the right colors, after a quit and after a fatal
error.

### 3. Mouse and keyboard

Without `+exec bringup.cfg`, add `+map q3dm1` and play. Check Ctrl alone fires,
Shift runs, and the mouse turns. `q3boot.log` lists the mouse's elements with
their kind and label. `using ... axis element(s)` means no device reports delta
movement, so axes are read as mouse movement, and the view may turn on its own.

### 4. Cinematic and menus

```text
+set net_noudp 1
```

This plays `idlogo.RoQ` and, until `com_introplayed` is saved, which needs a launch
like this one, `intro.RoQ` after it. Esc should skip each in turn. Then come the
menus. Without a `baseq3:q3key` file, the CD key screen comes first.

### 5. Sound

```text
+set net_noudp 1
+set s_initsound 1
```

Sound is off unless asked for. Check for clean menu sounds, `snd_restart` several
times at the console, and a clean quit.

### 6. Network

Leave out `net_noudp`, then try `connect` to a server. Esc gives up on a name
lookup. Try an ioquake3 server, then a retail one: the client starts on protocol
71 and falls back to 68.

The server browser's Internet list asks `sv_master1`, and `/globalservers 1 71` at
the console asks `sv_master2`. `+set cl_motd 1` asks id's update server at each
`connect`, and `+set com_authServer <host>` asks an authorize server. Neither is
asked by default: the old ones no longer answer, and the second is sent the CD key,
so that one can only be set on the command line.

`Dropped a packet with no source address` is harmless: such packets are not used,
and a game recovers. Send it along with its numbers, as the cause is not known.

## What to bring back

- `q3boot.log` and `q3boot.prev.log`, from every failure.
- The stage, and what was on screen.
- `aglDescribe` output from the console, if it gets that far.
- The heartbeat frame rate at each stage.

## Crashes

A fault writes `*** CRASH:` to `q3boot.log`, with the registers and callers, and
for a memory fault its address, and shows the error alert. On the build machine:

```sh
python3 mac_symbolize.py q3boot.log build_mac/Quake3.xcoff
```

prints the source line of each `code+0xNNNN` address. Trust `fault address` over
`DAR`, `DSISR` and `MSR`, which were wrong under emulation.

Not every bug is a fault: low memory is mapped, so a write through NULL carries on
regardless. And nothing stops the stack at once: deliberate runaway recursion ran
into the heap and hung, with no report.

With MacsBug installed, the handler should run first, though this is untested. To
stop in MacsBug at the fault instead, add `+set sys_crashhandler 0`. A line saying
r1 is below the heap limit usually means the 1 MB `cfrg` stack was too small, but
a fault on another stack, at interrupt time, reads the same.

| What happened | Look at |
| --- | --- |
| The Finder will not launch it | The library it names, or Get Info's memory |
| `Couldn't load default.cfg` | `pak0.pk3` is the demo's, or `baseq3` is not beside the application |
| `video memory` error | 256 colors, or no accelerated renderer for the desktop |
| Freeze, no alert | The end of `q3boot.prev.log` after one restart. Copy it before launching again |
| Machine resets or bombs, no `*** CRASH` | The fault was somewhere the handler cannot run. Try MacsBug |
| Freeze just after `*** CRASH` | A fault at interrupt time, in sound or networking, where the report cannot finish. Whatever it wrote before stopping is in the log |
| Black screen, then desktop | The end of `q3boot.log`, and stage 1 to rule out fullscreen |

## On ClassicMac

[ClassicMac](https://github.com/amcchord/ClassicMac) emulates a Radeon that Mac
OS 9's own ATI drivers run on, so AGL reports an accelerated renderer, which gets
past #268. It renders through the host's GPU, on macOS or Linux (EGL with
`EGL_MESA_platform_surfaceless`). Its Mac OS 9.2.2 Deluxe image has the drivers.

Every stage above has passed there on retail data: bots on three maps,
`vid_restart`, DrawSprocket and a window, InputSprocket, cinematics and menus,
Sound Manager including `snd_restart`, and Open Transport against ioquake3 servers
on protocols 71 and 68. So have real faults, the fatal alert, and a hard stop of a
frozen game with `q3boot.log` intact.

Put the game on a second disk rather than the boot disk, which is HFS+ in a
wrapper. An HFS volume can be filled, and its logs read back, with `hfsutils`.

| Seen there | What it means |
| --- | --- |
| Green cinematics, or everything green with `r_texturebits 32` | [ClassicMac #40](https://github.com/amcchord/ClassicMac/issues/40): the retail demo does the same. The port passes the right data |
| `r_fullscreen 0` starts fullscreen | The first accelerated `aglChoosePixelFormat` of a launch fails, and the fallback sets `r_fullscreen 1` and 16 bits, which reach `q3config.cfg`. Set `r_fullscreen 0` and `vid_restart` for a window. Not known whether a real card does this |
| `r_colorbits 32` ends up 16-bit | The same failure and fallback |

It says nothing about a real card's quirks, Rage Pro paths or real speed, and a
hard stop cannot lose what a real drive had cached, so the log still needs that
test on hardware.
