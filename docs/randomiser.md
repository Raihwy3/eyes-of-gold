# Randomiser

Open **Cheat Options** and turn **Randomiser** on before starting a mission. The entry is available without unlocking the original cheats.

The first pass changes ordinary guards’ bodies and guns, ordinary weapon pickups, existing starting guns and their ammunition, and the contents of ordinary ammo crates. It keeps enemy positions, routes and AI scripts. Reinforcements created later by scripts keep their original setup.

**New seed** shows the current six-digit seed. Select it to choose the next seed. The seed and toggle are saved in `native-options.ini`; restarting a mission with the same seed repeats its setup. You can also set `randomiser_seed` to a number from 1 to 999999 in that file while the game is closed. Changes take effect when the next mission loads.

Story characters also get random appearances, including their heads. Their original identities, scripts, allegiances, weapons and gameplay flags stay intact. The appearance pool is limited to two story models per stage. Keys, gadgets, explosives, armour and tagged or linked items stay intact. This is deliberately a restricted shuffle so it can be tested without moving objective items or changing mission scripts. Missions without an authored starting gun still start without one.

Each mission uses a pool of up to four firearm types and two ordinary male guard bodies. Weapon models and scale are taken from the ROM-backed engine tables. Ammo crates exchange their existing payloads, keeping ammunition types paired with their correct pickup models. There is no randomiser work in the frame loop, and it does not consume the original game’s random sequence.

Subsistence can be enabled alongside Randomiser. It still removes starting guns and ordinary ammunition; guards and pickups remain randomised. Original loadout-granting cheats are mutually exclusive with these modes. Randomiser uses the game’s normal cheat progression rules, so use a normal run for unlock progress. Multiplayer and attract demos are excluded.

Host checks cover the setups from all 20 campaign missions, repeatable and different seeds, preservation of mission items, story appearance swaps and their two-model limit, the cheat controls and saved settings. Console testing is still needed, especially for mission-specific behaviour and memory use on Old 3DS.
