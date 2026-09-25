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
With SideQuest's file manager (or Windows File Explorer), put your ROM file in the Quest's **Download** folder.

### 5. Launch the Game!
In the headset, open **App Library > Unknown Sources > Quest 3: OOT**. Allow file access if asked. The first launch builds the game data from your ROM: **you'll see loading dots for about 1 to 2 minutes**, once.

### 6. Play!

Congratulations, you are now sailing with the Ship of Harkinian, in VR! Have fun!

Full step-by-step guide: **[QUEST.md](QUEST.md)**.

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

How I installed it:
1. Unzipped the download on my computer. It's a folder of 37 files named `Djipi's 3DE - 01 ...` to `Djipi's 3DE - Z ...`.
2. Copied **every file** into the Quest's `SOHVR/mods` folder with SideQuest's file manager (no subfolders).
3. Launched the game, opened the settings menu (left menu button) and turned on **Settings > Mod Menu > Enable Mods**.
4. Played for a few minutes while the game optimized the pack for the Quest (**VR Settings > Performance > Texture Pack** shows the progress), then restarted the game.

Settings I use with it: **Enhancements > Graphics > Increase Actor Draw Distance** set to 4x (you can see across Hyrule Field) and **VR Settings > Performance > Stereo Render Divisor** at 2.

Optional files you can leave out: `24`/`25` (Majora's Mask style chests), `Original N64 HUD Mod` (keeps the N64 HUD), `Z - Crescent Moon Addon`. If you use a custom Link model, delete `02 Link's Textures`.

### About Djipi's 3DS Experience
A 3DS-style HD texture pack that looks great in VR. It isn't included (it's Djipi's work, made from Nintendo's textures): download it from [GameBanana](https://gamebanana.com/mods/477979) and copy its `.otr` files into `SOHVR/mods`.

The first time you play with it, the game **compresses the pack for the Quest in the background** (a few minutes; progress under **VR Settings > Performance > Texture Pack**). Restart when it says so: from then on it uses about 4x less memory and loads faster. Keep the original pack files installed.

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
