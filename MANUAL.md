# BitcoinClock Enclosure — User Manual

End-user guide for printing and assembling the BitcoinClock case for the
**Guition JC3248W535** board (ESP32-S3 + 3.5" touch display).

Designed by **silexperience**.

---

## 1. What you need

### Common to both versions

- The **Guition JC3248W535** board with its **4 original corner screws**
- A 3D printer, PLA or PETG filament
- USB-C cable (power / programming)
- Small Phillips screwdriver

### Version 2 (deep) additionally

- Small speaker (e.g. 8 Ω with JST 1.25 mm 2-pin plug — the board has a
  dedicated `Speak` connector)
- LiPo battery up to **2000 mAh** with JST 1.25 mm 2-pin plug (the board has a
  charge circuit and a `BAT` connector)
- **4× M3 × 8–10 mm self-tapping screws** (front frame ↔ rear shell junction)
- Foam double-sided tape or hot glue (to secure speaker and battery)

> ⚠️ **Safety — LiPo battery:** use a protected cell from a reputable brand,
> respect polarity, never pierce or crush the battery, and do not leave the
> device charging unattended for long periods.

> ⚠️ **The 4 corner screws also hold the screen frame.** Unscrew them gently,
> one at a time, while holding the screen in place.

---

## 2. Firmware (do this first — easiest before assembly)

1. Install the ESP32 Arduino core **2.0.14** and `GFX Library for Arduino`
   **v1.4.9 exactly** (+ ArduinoJson v7).
2. Compile & upload (adapt the port):

```bash
cd firmware/bitcoin-block-clock
arduino-cli compile --fqbn "esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,PartitionScheme=huge_app,CPUFreq=240,USBMode=hwcdc,CDCOnBoot=cdc" .
arduino-cli upload --fqbn "esp32:esp32:esp32s3" -p COM42 .
```

3. **Press RESET physically after flashing** (the board can stay in download
   mode while the COM port is held).
4. First boot: join the **`BlockClock-Setup`** WiFi AP (password `12345678`),
   enter your WiFi credentials → the clock reboots on your network as
   `http://blockclock.local`.

> The case does not block reflashing: on v2, remove the rear shell (4× M3)
> to reach the board; the boot/reset buttons are inside — flash **before**
> final assembly if you can.

## 3. Printing

### Version 1 — compact (`case/boitier_bitcoinclock.stl`)

| Setting | Value |
|---|---|
| Orientation | As supplied — front face flat on the bed |
| Supports | **Yes** — inside the inner cavity only (tree/organic recommended); marks stay hidden inside |
| Walls | 3–4 perimeters |
| Infill | 20–25 % |
| Layer height | 0.2 mm (0.16 mm for a smoother dome) |
| Material | ~35–45 g |

### Version 2 — deep (`case/boitier_deep_avant.stl` + `case/boitier_deep_arriere.stl`)

| Setting | Front frame | Rear shell |
|---|---|---|
| Orientation | As supplied — rear rim on the bed, opening up | As supplied — back face on the bed, opening up |
| Supports | **None** | **None** |
| Walls | 3–4 perimeters | 3–4 perimeters |
| Infill | 20–25 % | 15–25 % |
| Print time (approx.) | 3–4 h | 20–30 h (~158 mm tall) |

> 💡 15 cm is generous for a speaker + battery. For a shorter print, edit
> `TUBE_T_END` in `build_case_deep.py` (80–100 mm is plenty) and regenerate.

All STLs are verified watertight/manifold — your slicer should import them
with **zero repair**.

---

## 4. Assembly — Version 1

1. Unscrew the 4 corner screws of the board (one at a time, hold the screen).
2. Seat the board in the front pocket, **screen facing out**, USB-C on the
   **left**.
3. Refit the 4 original screws through the rear wells — the heads seat into
   the Ø6 mm counterbores. **Do not over-tighten** (1 mm well floor).
   If a screw feels too short, replace it with an M2.5/M3 screw 2–3 mm longer.
4. Plug USB-C through the side cutout.

![v1 posed](images/preview_pose.png)

---

## 5. Assembly — Version 2 (deep)

1. **Mount the board on the front frame** exactly as in v1 (steps 1–3 above).
   The screw wells are accessible from the rear of the frame while the shell
   is off.
2. **Connect the speaker** to the `Speak` JST connector (bottom edge) and the
   **battery** to the `BAT` JST connector (top area).
3. **Route the plugs** through the wire slots of the frame: bottom slot for
   speaker/IO, top slot for the battery.
4. Stick the **speaker** behind the rear grille and the **battery** inside the
   shell with foam tape or hot glue.
5. **Slide the rear shell** over the frame's boss until it seats on the
   shoulder, then fasten the **4× M3 screws** on the sides (Ø2.5 mm pilots are
   pre-drilled).
6. Plug USB-C on the left side of the frame.

![v2 assembled](images/preview_deep.png)

---

## 6. Using the clock

- **Power/charging:** plug USB-C on the left side. The board charges the
  battery automatically when present.
- **Battery switch:** the board's battery switch (rear side) becomes
  inaccessible once assembled — leave it **ON**, or open the shell (4× M3) to
  reach it. **Note:** a short press on this button is also required to *start*
  the board on battery (the power-management IC keeps the 5 V output off until
  then).
- **Battery gauge:** the header battery icon shows the % inside it and
  **blinks while charging**.
- **Voice (SAM):** the clock speaks English on new blocks
  (*"New block, \<pool\>"*). Try it: `http://blockclock.local/say?t=Hello`.
  Tune the voice with `SAM_SPEED`/`SAM_PITCH`/`SAM_MOUTH`/`SAM_THROAT` in the
  sketch, or revert to the plain bell with `SPEECH_BLOCKS 0`.
- **Sound:** night mode (23:00–07:00) silences event sounds; UI beeps remain.
- **Boot/Reset buttons:** also inside the case — open the shell to reprogram
  the board, or flash it before final assembly.
- The case leans back 12° with a full-length flat base: stable on any desk.

---

## 7. Troubleshooting

| Problem | Fix |
|---|---|
| Board doesn't seat fully | Check for filament blobs in the pocket corners; the pocket has +0.3 mm/side clearance |
| Original screw feels too short | Well floor is 1 mm by design; use an M2.5/M3 screw 2–3 mm longer |
| Screw holes don't line up | Pattern is 84.5 × 52.0 mm measured from photos — verify yours with calipers and adjust `HOLE_DX`/`HOLE_DY` in the build script |
| USB plug doesn't fit | Cutout is 11 × 5.2 mm; use a slim plug or slightly enlarge `USB_W`/`USB_H` |
| Case rocks (v1) | Base is stability-checked; ensure the bottom face printed flat (no elephant foot) — sand lightly if needed |
| Speaker sounds muffled (v2) | Make sure it sits against the grille slots, not against a solid wall |

## 8. Care

- Keep away from heat sources (>60 °C softens PLA).
- If the battery is installed, charge at least every few months.
- Clean with a dry cloth; no solvents on the printed parts.

---

*BitcoinClock enclosure — designed by **silexperience**.*
