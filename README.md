# The Legend of Zelda: Ocarina of Time VR (Quest Compatible)

# Important Information For Quest

- Runs **standalone on Meta Quest 3**. No PC, no Link cable, no streaming.
- Installed by sideloading (SideQuest). The full walkthrough is in **[QUEST.md](QUEST.md)**.
- You need your own copy of the game (ROM). No game assets are included.
- This is a Quest port of [Ship of Harkinian VR](https://github.com/ShinyWindow/Shipwright-VR) by ShinyWindow. For the PC VR version (DirectX 11, SteamVR), use his repo.

## Download

Get the latest APK from this repo's **[Releases](../../releases)** page.

## Support

For problems with the **Quest version**, open an [issue on this repo](../../issues). Ship of Harkinian's own [Discord](https://discord.com/invite/shipofharkinian) and [website](https://www.shipofharkinian.com/) are for the main (non-VR) game. Please keep in mind that we do not condone piracy.

# Quick Start

The Ship does not include any copyrighted assets. You are required to provide a supported copy of the game.

### 1. Verify your ROM dump
You can verify you have dumped a supported copy of the game by using the compatibility checker at https://ship.equipment/. If you'd prefer to manually validate your ROM dump, you can cross-reference its `sha1` hash with the hashes [here](docs/supportedHashes.json).

### 2. Turn on Developer Mode on your Quest (one time)
Create a free developer account at [developers.meta.com](https://developers.meta.com/horizon/), then in the **Meta Horizon** phone app go to **Devices > Headset settings > Developer mode** and turn it on.

### 3. Install the APK
Install [SideQuest](https://sidequestvr.com/setup-howto) on your computer, connect the Quest by USB (allow USB debugging in the headset), and use **Install APK** to install the `.apk` from [Releases](../../releases).

### 4. Copy your ROM to the Quest
With SideQuest's file manager (or Windows File Explorer), put your ROM file in the Quest's **Download** folder. Any file name works: the game recognises it by its contents.

### 5. Launch the Game!
In the headset, open **App Library > Unknown Sources > Quest 3: OOT**. When it asks, **check "allow access to edit files on device"**, then go back to the game. **The first boot takes about 1 to 2 minutes and only shows loading dots.** It isn't frozen: the game **finds your ROM by itself** and builds its data from it, which only happens once. Then it just starts. (If it can't find a ROM, a setup screen appears in the headset instead, where you can rescan and pick your ROM.)

### 6. Play!

Congratulations, you are now sailing with the Ship of Harkinian, in VR! Have fun!

Full step-by-step guide: **[QUEST.md](QUEST.md)**.

# Bonuses

Once the game is installed and running, two free upgrades make it look much better in VR. Both come from one download, **Djipi's 3DS Experience**, which isn't included in this project (it's Djipi's work, made from Nintendo's textures).

**Get the pack (for either bonus):** download it from [GameBanana](https://gamebanana.com/mods/477979) (the main download, `djipi_s_3ds_experience_tot_fix.zip`) and unzip it on your computer. You get a folder of 37 files named `Djipi's 3DE - 01 ...` to `Djipi's 3DE - Z ...`.

## Bonus 1: 3D rooms instead of green backdrops

Some places in Ocarina of Time (Link's house, the Market and its back alleys, the shops, the Temple of Time entrance) are flat pre-rendered pictures with the characters walking in front of them. That works on a TV but not in VR, so in these rooms you see **green backdrops** instead of walls. This bonus replaces them with real 3D rooms.

1. **Copy the room files to the Quest.** Connect the Quest, open SideQuest's file manager, go into **SOHVR**, then **mods**, and drag in these four files (just the files, no subfolders):
   - `Djipi's 3DE - 26 Background 3DS`
   - `Djipi's 3DE - 27 Background Textures`
   - `Djipi's 3DE - 32 Background Full 3D (OPTIONAL)`
   - `Djipi's 3DE - 33 Background Full 3D Textures (OPTIONAL)`
2. **Turn the mods on.** Launch the game, press the **hamburger button** (left menu button) to open the settings, and turn on **Settings > Mod Menu > Enable Mods**.
3. **Walk into Link's house.** It's a real 3D room now.

**Still seeing green backdrops, or a room looks wrong?** Check that files `32` and `33` are directly in `SOHVR/mods` (not in a subfolder) and that **Enable Mods** is on. The rooms are tested with the whole pack installed; if something looks off with only these four files, add the rest (Bonus 2).

## Bonus 2: HD textures (the 3DS look)

Replaces the whole game's N64 textures and models with Ocarina of Time 3D's HD versions: characters, enemies, items, dungeons and Hyrule Field.

1. **Copy the rest of the pack.** In SideQuest's file manager, drag **all 37 files** into **SOHVR/mods** (the four room files from Bonus 1 are part of it, so this covers both bonuses).
2. **Turn the mods on** (same as above): **Settings > Mod Menu > Enable Mods**.
3. **Let it optimize.** The first time you play with the pack, the game compresses it for the Quest in the background, which takes about 3 minutes. You can keep playing; the progress is in **VR Settings > Performance > Texture Pack**.
4. **Restart once.** When it says "restart the game to use it", quit and launch again. From then on it uses about 4x less memory and loads faster.

Keep Djipi's original files in the mods folder: the game still reads a few textures from them. If you add, remove or update pack files later, it notices and re-optimizes by itself.

Optional files you can leave out: `24`/`25` (Majora's Mask style chests), `Original N64 HUD Mod` (keeps the N64 HUD), `Z - Crescent Moon Addon`. If you use a custom Link model, delete `02 Link's Textures`.

# Set it up like mine (step by step)

**One-time Quest setup**
- Create a free developer account at [developers.meta.com](https://developers.meta.com/horizon/).
- In the **Meta Horizon** phone app: **Devices > Headset settings > Developer mode**, turn it on, then restart the Quest.
- On a computer, install **SideQuest (Advanced Installer)** from [sidequestvr.com](https://sidequestvr.com/setup-howto).
- Plug the Quest in by USB, put it on, and choose **Allow** (tick "always allow") for USB debugging.

**Install the game**
- Download the latest `Quest3-OOT-*.apk` from [Releases](../../releases).
- In SideQuest, click **Install APK** and pick that file.
- In SideQuest's file manager, drag your own Ocarina of Time ROM (`.z64`) into the Quest's **Download** folder.

**First launch**
- In the headset: **App Library > Unknown Sources > Quest 3: OOT**.
- When it asks, **check "allow access to edit files on device"**, then go back to the game.
- **The first boot takes about 1 to 2 minutes and only shows loading dots.** Don't worry, it isn't frozen: it's building the game's data from your ROM, which only happens once.

**3D rooms and HD textures (Djipi's 3DS Experience, all 37 files)**
- Follow [Bonuses](#bonuses) above: I use both.

**Settings**
- **VR Settings > Comfort & Movement:** turning set to **Smooth**.
- **VR Settings > Performance > Stereo Render Divisor:** **2** (the default; see below).
- **Enhancements > Graphics > Increase Actor Draw Distance:** **5x**.
- **Left-handed mode:** off (right hand is the sword hand).
- **VR Settings > VR Inputs** (selector profile): left grip = **Z-target**, right grip = **shield (R)**, **Y = pause (Start)**, left stick click = **L**.
- **Extra Settings Menu Button:** **None** (the hamburger always opens settings).
- **VR Settings > Items & Archery:** Landing Target **on**, Flight Path Line **off**.

**What the Stereo Render Divisor does:** at **1**, the game redraws the 3D world for every frame the headset shows (72 per second), so animation and movement are fully smooth. At **2**, it redraws the world every other frame, and the headset fills the frames in between by shifting the last image to match your head. Head tracking stays at full speed either way, but the world itself animates at half rate, and the game needs about half the processing power. The default is **2**, which keeps busy areas smooth. Try 1 if you want fully smooth world animation and it holds up in the areas you play.

# Configuration

### Default VR controls
| Action | Control |
| - | - |
| Move | Left stick |
| Turn | Right stick (smooth turning; snap turning is in the settings) |
| Sword | Swing your sword hand |
| Shield | Raise your shield hand |
| Choose an item | Click the right stick, flick your hand toward an item, release |
| Use an item | Trigger |
| Slingshot / bow | Bring your free hand to the weapon, squeeze the trigger, pull back, release. A ring shows where the shot will land (target and flight line toggles: VR Settings > Items & Archery). |
| Grab and throw a Deku Nut | Grip near the nut in front of your chest, then throw |
| Pause, save, equip | Left stick click (right stick click in left-handed mode) |
| Settings menu (in the headset) | Left menu button (a second button can be added in Settings > Controls); aim with a controller, trigger to click |

### VR settings
Everything VR-specific (left-handed mode, turning, hand and HUD placement, comfort, performance) is under **VR Settings** in the in-headset menu.

# Project Overview
Ship of Harkinian (SOH) is built atop a custom library dubbed libultraship (LUS). Back in the N64 days, there was an SDK distributed to developers named libultra; LUS is designed to mimic the functionality of libultra on modern hardware. In addition, we are dependant on the source code provided by the OOT decompilation project.

Ship of Harkinian VR adds a VR renderer and motion controls on top. This Quest port runs that VR renderer natively on the headset:

- **OpenXR on OpenGL ES** (Meta's Android runtime) instead of Direct3D 11 and OpenVR.
- **Single-pass stereo** (both eyes rendered in one pass with GL_OVR_multiview2), plus an optimized software renderer, holding 72 fps in Hyrule Field.
- **Quest-first features:** physical slingshot and bow with a landing marker, 3D item compass, in-headset settings menu, whole-view scene fades, hand-locked held items.
- **On-device texture-pack optimization:** HD packs are compressed to ASTC (the Quest GPU's native format) automatically.

The full list is in [CHANGES.md](CHANGES.md).

In order for the game to function, you will require a **legally acquired** ROM for Ocarina of Time. Click [here](https://ship.equipment/) to check the compatibility of your specific rom. Any copyrighted assets are extracted from the ROM and reformatted as an archive file which the code uses.

# Custom Assets

Custom assets are packed in `.otr` / `.o2r` archive files. To use custom assets on the Quest, place them in the **`SOHVR/mods`** folder on the headset (with SideQuest's file manager) and turn on **Settings > Mod Menu > Enable Mods**.

### The mods I play with

| Mod | Where to get it | What it does |
| - | - | - |
| **Djipi's 3DS Experience** (all 37 files, including the optional ones) | [GameBanana](https://gamebanana.com/mods/477979), main download `djipi_s_3ds_experience_tot_fix.zip` | Replaces the N64 textures, models and backgrounds with Ocarina of Time 3D's HD look. The "Background Full 3D" files turn the pre-rendered rooms (Link's house, the Market back streets, shops) into real 3D rooms, which matters a lot in VR. |

How to install it: see [Bonuses](#bonuses).

Settings I use with it: **Enhancements > Graphics > Increase Actor Draw Distance** set to 5x (you can see across Hyrule Field) and **VR Settings > Performance > Stereo Render Divisor** at 2.

It isn't included in this project (it's Djipi's work, made from Nintendo's textures), which is why you download it yourself.

If you're interested in creating and/or packing your own custom asset files, check out the following tools:
* [**retro - OTR generator**](https://github.com/HarbourMasters64/retro)
* [**fast64 - Blender plugin**](https://github.com/HarbourMasters/fast64)

# Development
### Building

The Android/Quest project is in `Android/`. Clone with submodules (`git clone --recursive`), install JDK 17 and the Android SDK and NDK, then run `./gradlew assembleDebug` in `Android/`. For the desktop builds, see the [building instructions](docs/BUILDING.md).

The VR layer lives in `libultraship/src/fast/vr_openxr.cpp`; the VR gameplay features are in `soh/soh/Enhancements/vr-combat/`.

### Further Reading
More detailed documentation can be found in the 'docs' directory, including the aforementioned [building instructions](docs/BUILDING.md).

* [Credits](docs/CREDITS.md)
* [Custom Music](docs/CUSTOM_MUSIC.md)
* [Controller Mapping](docs/GAME_CONTROLLER_DB.md)
* [Modding](docs/MODDING.md)
* [Versioning](docs/VERSIONING.md)

# Credits

- **HarbourMasters** for Ship of Harkinian.
- **ShinyWindow** for Ship of Harkinian VR.
- **linkzenic** for the Android port of Ship of Harkinian.
- **Djipi** for the 3DS Experience texture pack.
- Quest 3 standalone port by **ChocoTorp**.

This is a fan project, not affiliated with or endorsed by Nintendo. The Legend of Zelda and Ocarina of Time are trademarks of Nintendo.
