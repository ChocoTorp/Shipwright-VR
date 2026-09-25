# The Legend of Zelda: Ocarina of Time VR (Quest Compatible)

Play Ocarina of Time in full VR, standalone on a **Meta Quest 3**. No PC or cable needed once it's installed.

This is a Quest port of [Ship of Harkinian VR](https://github.com/ShinyWindow/Shipwright-VR) by ShinyWindow, which is built on [Ship of Harkinian](https://github.com/HarbourMasters/Shipwright) by HarbourMasters. It adds standalone Quest support (OpenXR on Android), single-pass stereo rendering and a set of Quest-specific features. See [CHANGES.md](CHANGES.md) for the full list.

**This project contains no game assets.** You need your own legally dumped copy of the game (the ROM).

## What you need

- A Meta Quest 3 (Quest 3S and Quest Pro should work but are untested).
- A computer (Windows, Mac or Linux) and a USB-C cable to connect the Quest.
- Your own Ocarina of Time ROM (`.z64`, `.n64` or `.v64`). Check it's a supported version at [ship.equipment](https://ship.equipment/).

## Install, step by step

### 1. Turn on Developer Mode (one time)
1. Go to [developers.meta.com](https://developers.meta.com/horizon/) and create a free developer account (it asks you to create an "organization"; any name works).
2. Open the **Meta Horizon** app on your phone, go to **Devices**, select your Quest, then **Headset settings**, then **Developer mode**, and turn it **on**.
3. Restart the Quest.

### 2. Install SideQuest on your computer
1. Download **SideQuest (Advanced Installer)** from [sidequestvr.com/setup-howto](https://sidequestvr.com/setup-howto) and install it.
2. Connect the Quest to your computer with the USB cable.
3. Put the headset on and choose **Allow** (tick "Always allow from this computer") when it asks about USB debugging.
4. SideQuest shows a green dot in the top left when the Quest is connected.

### 3. Install the game
1. Download the latest `Quest3-OOT-*.apk` (older releases: `ShipOfHarkinianVR-Quest3-*.apk`) from this repo's [Releases](../../releases) page.
2. In SideQuest, click the **Install APK** button (the box with a down arrow, top right) and pick the APK. Wait for "Success".

### 4. Copy your ROM to the Quest
1. In SideQuest, open the **file manager** (the folder icon, top right).
2. Open the **Download** folder and drag your ROM file into it.
   (On Windows you can also use File Explorer: **Quest 3 > Internal shared storage > Download**.)

### 5. First launch
1. In the headset, open the **App Library**, change the filter from **All** to **Unknown Sources**, and start **Quest 3: OOT**.
2. When it asks, **check "allow access to edit files on device"**, then go back to the game. It stores its data in a folder called `SOHVR`.
3. **The first boot takes about 1 to 2 minutes and only shows loading dots.** Don't worry, it isn't frozen: it's building the game's data from your ROM, which only happens once. Every launch after that is quick.
4. The game starts. Have fun!

## Optional: HD textures (Djipi's 3DS Experience)

The game looks great with Djipi's 3DS-style texture pack. It isn't included here (it's Djipi's work, made from Nintendo's textures), so download it yourself:

1. Download **Djipi's 3DS Experience** from GameBanana: [gamebanana.com/mods/477979](https://gamebanana.com/mods/477979) and unzip it on your computer.
2. In SideQuest's file manager, open **SOHVR**, then **mods**, and drag all the `.otr` files from the pack into it.
3. Launch the game. In the settings menu (left menu button), go to **Settings > Mod Menu** and make sure **Enable Mods** is on.
4. **Automatic Quest optimization:** the first time you play with the pack, the game compresses it for the Quest's GPU in the background while you play. It takes a few minutes. You can watch the progress in **VR Settings > Performance > Texture Pack**. When it says "restart the game to use it", quit and relaunch. From then on the pack uses about 4 times less memory and areas load faster. Keep Djipi's original files installed: the game still reads a few textures from them.

If you add, remove or update pack files later, the game notices and re-optimizes automatically.

## Controls (default)

| Action | Control |
|---|---|
| Move | Left stick |
| Turn | Right stick (smooth turning; snap turning is in the settings) |
| Sword | Swing your sword hand |
| Shield | Hold up your shield hand |
| Pick an item | Click the right stick, flick your hand toward an item, release |
| Use an item | Trigger |
| Slingshot / bow | Bring your free hand to the weapon, squeeze the trigger, pull back, release. A ring shows where the shot will land. |
| Grab / throw a Deku Nut | Grip near the nut in front of your chest, throw |
| Pause, save, equip | Left stick click (right stick click in left-handed mode) |
| In-headset settings menu | Left menu button; a second button can be added in Settings > Controls (aim with a controller, trigger to click) |

Left-handed mode, hand position, HUD placement and more are in **VR Settings**.

## Troubleshooting

- **Stuck on loading dots for more than 3 minutes on first launch:** make sure your ROM is in the Quest's **Download** folder (or in `SOHVR`) and is a supported version, then restart the game.
- **Can't find the game in the headset:** it's under **App Library > Unknown Sources**.
- **HD textures don't show:** check **Settings > Mod Menu > Enable Mods**, and that the `.otr` files are directly in `SOHVR/mods`.

## Building from source

The Android project is in `Android/`. Clone with submodules (`git clone --recursive`), install JDK 17 and the Android SDK/NDK, then run `./gradlew assembleDebug` in `Android/`. The game's native code, the VR layer (`libultraship/src/fast/vr_openxr.cpp`) and the Quest-specific changes are all in this branch and its submodules.

## Credits

- **HarbourMasters** for Ship of Harkinian.
- **ShinyWindow** for Ship of Harkinian VR (motion controls, physical combat, the VR renderer).
- **linkzenic** for the Android port of Ship of Harkinian.
- **Djipi** for the 3DS Experience texture pack.
- Quest 3 standalone port by **ChocoTorp**.

This is a fan project, not affiliated with or endorsed by Nintendo. The Legend of Zelda and Ocarina of Time are trademarks of Nintendo.
