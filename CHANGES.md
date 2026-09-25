# QuestShip Changes

## Visual

- Game renders in native stereo 3D on Quest 3.
- HD texture pack geometry and fonts now display correctly.
- Mipmaps and anisotropic filtering stop distant textures from shimmering.
- Distant objects no longer vanish when looked at directly.
- Draw distance raised four times for wide open views.
- Item compass shows small spinning 3D models of items.
- Deku Nuts appear as real 3D nuts in hand.
- Held nuts sit in the palm and render correctly.
- Hands are 25 percent smaller and scaled around palms.
- Sword pivots from the wrist like the original mod.
- Ammo counter floats at the nock, locked to hand.
- Landing ring shows exactly where each shot will hit.
- Optional flight path line is smooth and steady width.
- Slingshot aim tilts up three degrees and launches higher.
- Hand-held models and markers no longer trail while walking.
- Items near your face stay visible in both eyes.
- Fixed rare black textures caused by stale mipmap state.
- Soft resets can no longer glitch the title screen.
- Bombs, Zelda's Letter and trade items show as 3D.


## Controls

- Smooth turning replaces snap turning as the default setting.
- In-headset settings menu with a laser pointer to click.
- Left menu button opens settings; stick click pauses game.
- Settings menu blocks gameplay buttons while it is open.
- Item compass locks in space so walking never misselects.
- Compass models are ignored by sword and item collision.
- Nut preview waits in front of your chest, space-locked.
- Add a second button to open the settings menu.
- Archery landing target and flight line have separate switches.


## Optimization

- Renderer now compiled optimized; Hyrule Field holds 72 fps.
- Single-pass stereo renders both eyes with one interpreter pass.
- Stereo divisor setting can halve world redraws when needed.
- Persistent mapped vertex ring removes per-draw GPU memory allocation.
- Resource lookups are cached per frame, verified by name.
- HD textures preload in the background on idle cores.
- Texture pack compressed to ASTC, four times less memory.
- Trace logging compiled out, stopping thousands of log lines.
- Color combiner cache key fixed, ending random shader lookups.
- Unchanged uniforms and sampler settings are no longer resent.
- Vertex and texture math hoisted out of inner loops.
- Repeated identical tile setups no longer force texture reimports.
- Display list commands skip a shared pointer lock each.
- Setup runs off the main thread, preventing startup freezes.

---

# Details

## Visual

**Game renders in native stereo 3D on Quest 3.**
The PC mod drew through Direct3D 11 and OpenVR, which do not exist on a standalone headset. The VR layer now talks to the headset through OpenXR, sharing the game's OpenGL ES context with the Khronos Android loader. Each eye renders into its own OpenXR swapchain image, and the HUD, flat screens and menu become floating quad layers. The APK carries a VR manifest, so it launches as an immersive app instead of a flat window.

**HD texture pack geometry and fonts now display correctly.**
Android on ARM64 tags heap pointers with a marker in the top byte (0xB4). Two address range checks in the renderer mistook those tagged pointers for invalid ones and skipped the data behind them. That hid mod geometry (Link's house was only a skybox) and turned dialog fonts into noise. Both checks now strip the tag before comparing, so every mesh and glyph loads.

**Mipmaps and anisotropic filtering stop distant textures from shimmering.**
Without mipmaps, far textures sample single texels that change as your head moves, which reads as crawling noise in VR. Every uploaded texture now gets a full mip chain, built on the GPU or prebuilt in the ASTC pack. Trilinear filtering blends the levels, and 4x anisotropy keeps floors sharp at grazing angles. Tiny textures skip the chain because it gains them nothing.

**Distant objects no longer vanish when looked at directly.**
The original game culls actors by depth along the camera's view axis. On a TV that is invisible, but with a turnable head an object off to the side had less depth and stayed drawn, then vanished when you looked straight at it. In VR the test now uses the straight-line distance from your eyes instead. What is drawn no longer depends on where you look.

**Draw distance raised four times for wide open views.**
Ship of Harkinian's actor draw distance multiplier is set to 4 in the Quest config. Trees, enemies and scenery now stay visible much farther across Hyrule Field. The single-pass stereo and compiler work made this affordable. It can be changed in the settings menu if a scene gets heavy.

**Item compass shows small spinning 3D models of items.**
Clicking the right stick opens the item compass, which used to show flat icons. Each slot now shows the same 3D model Link holds overhead when he gets an item, at 20 percent of that size. The selected item grows 30 percent larger so the choice is obvious. Items without a model and empty slots keep a flat marker.

**Deku Nuts appear as real 3D nuts in hand.**
In the original game a thrown Deku Nut is a dark spinning sparkle and a held one draws nothing. In VR the nut now uses the 3D get-item nut model, both in your hand and while it flies. It tumbles along its flight with a smooth spin that never jumps. The model is display only and is excluded from sword collision.

**Held nuts sit in the palm and render correctly.**
The hand model's origin is the wrist joint, so a grabbed nut first appeared at your wrist. The nut now offsets itself to the palm point, using the same palm setting the hand scaling uses. The sword hand is a mirrored model, which turned the nut inside out. The nut's draw now undoes that reflection so its faces point outward.

**Hands are 25 percent smaller and scaled around palms.**
Link's hands looked oversized in first person. Only the hand mesh shrinks, to 75 percent by default, and it scales around the palm so the fist stays where the sword grip is. Held items are not scaled, so the sword and shield keep their normal size. The amount is a setting, gVrHandMeshScale.

**Sword pivots from the wrist like the original mod.**
Moving the grab point to the palm briefly shifted the whole hand frame, which pulled the sword closer and moved its pivot. The hand frame is back to wrist-origin, so the sword sits and swings exactly as it did before. Only the grabbed nut offsets to the palm now. Physical sword collision matches what you see again.

**Ammo counter floats at the nock, locked to hand.**
The nock marker on the slingshot and bow now shows how much ammo is left, using the HUD's own digit font. It turns red at zero and grows a little when your string hand is close enough to nock. It is attached to the weapon hand at headset rate, so it never lags behind the slingshot. Its size is a setting in the menu.

**Landing ring shows exactly where each shot will hit.**
While the string is drawn, a flat ring lies on the surface the shot will hit. The flight is simulated with the projectile's real speed, gravity timing and collision, so the ring is accurate. It sits flat on floors and walls, grows with distance so far targets stay visible, and glides smoothly between game ticks. No ring means the shot would hit nothing in range.

**Optional flight path line is smooth and steady width.**
The older trajectory line is still available as a menu toggle. It is now a smoothed curve through the simulated points instead of a jagged polyline. Its vertices are stored at high precision, which fixes the width flicker caused by rounding to whole units. It fades in near the pouch and fades out evenly toward the landing point.

**Slingshot aim tilts up three degrees and launches higher.**
The slingshot felt like it aimed slightly downward compared with how it is held. The aim direction now pitches up 3 degrees and the shot launches 6 centimeters higher, from between the tines. Both values are settings. The seed itself no longer appears on the slingshot.

**Hand-held models and markers no longer trail while walking.**
The game draws at 20 ticks per second, so anything positioned once per tick lagged behind your real hand at 72 fps. The renderer now keeps a registry of matrices welded to a hand or to a spot in physical space. Each frame it rebuilds them from the live hand or head pose. The nut, ammo counter, trajectory line and compass all use this, so they move with you exactly.

**Items near your face stay visible in both eyes.**
Single-pass stereo decides on the CPU what to cull using one combined view. That view sat between your eyes, so each eye could see slightly past it and objects close to your face at the edges disappeared for one eye. The combined view's point now sits a few centimeters behind your eyes, which makes it contain both eyes' views. Held items at the edge of vision stay solid.

**Fixed rare black textures caused by stale mipmap state.**
Texture filtering was set before a texture's pixels were uploaded, when it still described whatever that texture slot held before. A reused slot could then ask for mip levels that did not exist, which makes the GPU sample black. The upload now sets the filter and mip range to match its own contents. Framebuffer textures get the same reset.

**Soft resets can no longer glitch the title screen.**
The hand and space matrix registries are keyed by memory addresses that the game reuses. They were only cleared during gameplay drawing, so a reset with the item compass open left stale entries behind. Those entries could replace title or file select matrices with a hand pose. The registries are now also cleared whenever the game changes state.

**Bombs, Zelda's Letter and trade items show as 3D.**
Bombs waiting in front of your chest are now the real 3D bomb, like the Deku Nuts. Zelda's Letter, the eggs, the chickens and every trading-sequence item appear as 3D models in your hand. They are welded to the live hand pose, so they turn with your wrist and never trail. When you run out of nuts or bombs, the waiting model turns 30 percent see-through instead of disappearing.

## Controls

**Smooth turning replaces snap turning as the default setting.**
Right stick turning now rotates smoothly instead of jumping in steps. It is set in the Quest config with gVrTurnStyle. Snap turning is still available in the settings menu for anyone who prefers it. The item compass and other space-locked elements follow the turn correctly.

**In-headset settings menu with a laser pointer to click.**
The Ship of Harkinian settings menu now renders onto a panel floating in front of you. The aim ray from your controller acts as a mouse, with the trigger to click and the stick to scroll. The panel is sized for reading in the headset and closes with the same button. Every setting from the PC menu is reachable without taking the headset off.

**Left menu button opens settings; stick click pauses game.**
The left menu button toggles the in-headset settings menu and is never passed to the game. Pause, save and equip stay on the left stick click, as in the original VR mod. An unfinished tap-to-pause path was removed so the behavior is simple and predictable. Left-handed setups that move the selector to the left stick currently have no pause button.

**Settings menu blocks gameplay buttons while it is open.**
Clicking a menu entry used to also fire the bow, grab a nut or open the item compass behind the panel. Gameplay code now reads buttons through a gate that returns nothing while the menu is open. Button binding capture in the menu still sees the real buttons. Closing the menu hands the controllers straight back to the game.

**Item compass locks in space so walking never misselects.**
The compass used to follow Link's body in game space, so walking or turning while it was open read as a flick and picked the wrong item. It now anchors in physical tracking space where your hand was when you opened it. Only real hand movement relative to that spot selects an item. It renders at headset rate, so it holds perfectly still while you move.

**Compass models are ignored by sword and item collision.**
Physical combat builds collision from what is drawn, so the compass models could block or catch the held item. The compass, the nut models, the ammo counter and the trajectory are all masked out of that collision pass. They are purely visual. Real world geometry still collides as before.

**Nut preview waits in front of your chest, space-locked.**
When Deku Nuts are equipped, a 3D nut waits in front of your chest to be grabbed. It is placed in physical space relative to your head, 40 centimeters ahead and 25 centimeters down. It rides with the headset at render rate, so it does not trail while walking. It turns slowly so it reads as an object to pick up.

**Add a second button to open the settings menu.**
The left menu button (the hamburger) always opens the settings menu. A new option in Settings, Controls and in VR Settings, VR Inputs adds a second button: a stick click, X, Y, A or B. That button then also opens and closes the menu and is no longer passed to the game. It is off by default.

**Archery landing target and flight line have separate switches.**
The slingshot and bow landing target (ring and dot) and the flight path line are now two independent settings. Both live in VR Settings under Items and Archery, in an Archery Targeting section. You can show either one, both or neither. The target is on and the line is off by default.

## Optimization

**Renderer now compiled optimized; Hyrule Field holds 72 fps.**
The build told the game code to optimize, but a root build file overrode it for the renderer, SDL and every other library. The software N64 renderer, which does about 93 percent of the game thread's work, was running completely unoptimized. With -O2 on everything, and the work below, Hyrule Field went from 7 to 13 fps at the start of the port to a locked 72. The headset even lowered its clocks because it had power to spare.

**Single-pass stereo renders both eyes with one interpreter pass.**
The game's graphics run through a software N64 interpreter, which used to run once per eye. Now it runs once, writing world-space vertices, and a multiview shader applies each eye's view on the GPU. Both eyes draw into one two-layer image in a single pass. This roughly halved the CPU cost of rendering.

**Stereo divisor setting can halve world redraws when needed.**
With gVrStereoDivisor set to 2, the world redraws every other headset frame. The frames in between reuse the last image and the headset reprojects it to your current head pose. Head tracking stays at full rate, only world animation drops to 36 fps. With the new headroom, divisor 1 (full 72 fps animation) is the next thing to try.

**Persistent mapped vertex ring removes per-draw GPU memory allocation.**
Every draw used to upload vertices with glBufferData, which made the Quest driver allocate GPU memory each time. That cost about 13 percent of the game thread. Now one large buffer is mapped once and draws are written into it as a ring, guarded by fences so the GPU is never overwritten mid-read. A fence that runs long is waited on rather than skipped.

**Resource lookups are cached per frame, verified by name.**
Mod meshes and textures are referenced by file path, and each reference searched the resource cache again for every object, eye and frame. That was about 26 percent of the game thread in Hyrule Field. Lookups are now remembered and dropped whenever the resource manager changes anything. Entries keyed by a string's address also check the text, which fixed garbled dialog.

**HD textures preload in the background on idle cores.**
Entering an area used to stall while every HD texture was read and decompressed on the render thread. When a scene loads, its HD textures are now queued on the idle worker threads at low priority. The texture archive keeps a small pool of independent file handles so reads run in parallel safely. Queued work is cancelled cleanly on shutdown or when the scene changes.

**Texture pack compressed to ASTC, four times less memory.**
An offline converter turns the HD pack's raw textures into ASTC, the Quest GPU's native compressed format, with prebuilt mip chains. 9,720 textures went from 2,133 MB to 534 MB of GPU memory at a mean quality of 49 dB. Textures the renderer must edit on the CPU fall back to the original pack automatically. The converter and loader validate every file, so a bad or missing pack degrades to blank textures, never a crash.

**Trace logging compiled out, stopping thousands of log lines.**
The debug build logged at its most detailed level, up to 1,400 lines per second during loads. Writing those lines to storage cost real time on the game thread. Logging below the info level is now compiled out entirely. Warnings and errors still reach the log file.

**Color combiner cache key fixed, ending random shader lookups.**
Every triangle builds a key to find its color combiner, and one field of that key was never initialized. The key then compared unequal at random, forcing a map search per triangle and sometimes creating duplicate combiners. The key is now zero initialized. This bug came from upstream and affects every platform.

**Unchanged uniforms and sampler settings are no longer resent.**
Each draw used to resend the stereo matrices, depth and texture size uniforms, and each texture bind resent its filter and wrap modes. Every program and texture now remembers what it was last sent. Only values that actually changed go to the driver. That removes several OpenGL calls from each of thousands of draws per frame.

**Vertex and texture math hoisted out of inner loops.**
The vertex loop reloaded matrices, flags and settings for every vertex, and the triangle loop divided texture coordinates per vertex. Those values are now computed once per batch and kept in registers. Texture coordinates become one multiply and add, and colors use a multiply instead of a divide. Results match to the last bit that matters.

**Repeated identical tile setups no longer force texture reimports.**
Display lists often repeat the same texture tile settings, and each repeat marked textures as changed. That flushed the current batch and re-ran the texture import on the next triangle. Tile commands now only mark a change when a value actually differs. Fewer flushes means fewer, larger draw calls.

**Display list commands skip a shared pointer lock each.**
Every display list command handler locked a weak pointer to find the renderer, which costs two atomic operations. At thousands of commands per frame this showed up as 2.3 percent of the game thread. Handlers now use a plain pointer that is valid for the whole pass. The behavior is identical.

**Setup runs off the main thread, preventing startup freezes.**
First launch deletes and copies support files, which took about 13 seconds and triggered Android's "not responding" dialog. That work now runs on a background thread while the UI stays responsive. If setup ever fails, the game is released instead of waiting forever on a black screen. The extracted game archive is kept between installs so extraction happens once.
