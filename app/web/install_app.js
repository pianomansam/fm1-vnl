/* SPDX-License-Identifier: GPL-3.0-or-later
   VNL-1's steps around a browser install (fm1.toml install_app; fm1-base web/installer.html APP): the sample bank,
   written after the firmware, as tools/fm1_bank.py writes it. The bank is NAME-VERSION-bank.bin beside the package on
   the site (tools/site.sh puts it there, from the same checkout as the package). The protocol is app.c's app_usb_cmd:
   BANK_CMD with the bank's length and CRC-32, then the FM-1 pulls the bank with read requests and reports with a
   read of BANK_OK, or BANK_FAIL and an error. Uses fm1ota.js's helpers, which the installer page inlines: updater,
   frame, parseRequest, response, crc32, le32, IS_OTA. */
({
  /* before anything is written: load and check the bank, then ask whether to write it -> {bank} (null: skip) */
  async before(midi, ui) {
    const url = meta.pkg.replace(/\.fwsc$/, "-bank.bin");
    let bank = null;
    try {
      const r = await fetch(url);
      if (!r.ok) throw new Error(`HTTP ${r.status}`);
      bank = new Uint8Array(await r.arrayBuffer());
      const rd = (o) => (bank[o] | bank[o + 1] << 8 | bank[o + 2] << 16 | bank[o + 3] << 24) >>> 0;
      const magic = String.fromCharCode(...bank.subarray(0, 4));
      if (bank.length < 16 || magic !== "VNLB" || rd(4) !== 2 || rd(8) !== bank.length - 16 ||
          crc32(bank.subarray(16)) !== rd(12)) throw new Error("not a VNL-1 sample bank, or a damaged one");
    } catch (e) {
      ui.log(`sample bank: ${url}: ${e.message}`);
      const go = await ui.ask("Sample bank missing",
        "The sample bank did not load, so only the firmware will be installed. VNL-1 then plays its synth " +
        "instruments only (NO SAMPLES) until the bank is written with <code>tools/fm1_bank.py</code>.",
        [["go", "Install the firmware"], ["stop", "Cancel"]]);
      return go === "go" ? { bank: null } : false;
    }
    const kb = Math.round(bank.length / 1024);
    const go = await ui.ask("Sample bank",
      `VNL-1's sampled instruments (piano, guitars, cello, harp and the two room kits) are a separate ${kb} KB ` +
      "sample bank. A firmware update leaves it in place, so write it on the first install, or when this " +
      "release's notes say the samples changed. It takes about a minute, after the firmware.",
      [["bank", "Firmware and sample bank"], ["fw", "Firmware only"]]);
    return { bank: go === "bank" ? bank : null };
  },

  /* once the new firmware runs: write the bank; a failure is said, the firmware stays installed */
  async after(midi, ui, state) {
    if (!state || !state.bank) return;
    const bank = state.bank;
    const ERR = { 2: "a read request went unanswered", 3: "this bank is not the one this firmware was built with",
                  7: "a flash erase failed", 8: "a flash write failed", 9: "the check after writing failed",
                  11: "the bank holds a mark the update loader would misread" };
    for (;;) {
      ui.say("Writing the sample bank: waiting for VNL-1 to start");
      ui.progress(0);
      const dev = await updater(midi, ui.log).waitFor((id) => !IS_OTA(id) && id.text === meta.product, 40000);
      if (!dev) throw new Error("VNL-1 did not come back to write the sample bank: switch the FM-1 off and on, " +
                                "then write it with tools/fm1_bank.py");
      const { link } = dev;
      let code;
      try {
        link.drain();
        link.send(frame(0x60, [...le32(bank.length), ...le32(crc32(bank))]));
        let last = Date.now();
        for (;;) {
          const pkt = await link.read(500);
          if (!pkt) {
            if (link.lost) throw new Error("the FM-1 was disconnected while writing the sample bank");
            if (Date.now() - last > 20000) throw new Error("the FM-1 stopped asking for the sample bank");
            continue;
          }
          const r = parseRequest(pkt);
          if (!r) continue;
          last = Date.now();
          const top = (r.addr & 0xFF000000) >>> 0;
          if (top === 0xC0000000 || top === 0xCF000000) {
            link.send(response(r.addr, [...new TextEncoder().encode("success"), 0], r.fl));
            code = top === 0xC0000000 ? 0 : (r.addr & 0xFFFF) || 99;
            break;
          }
          if (r.addr + r.len > bank.length) throw new Error(`bad request ${r.addr.toString(16)}+${r.len}`);
          if (!link.send(response(r.addr, Array.from(bank.subarray(r.addr, r.addr + r.len)), r.fl)))
            throw new Error("the FM-1 was disconnected while writing the sample bank");
          ui.progress(((r.addr + r.len) * 100) / bank.length);
        }
      } finally { link.close(); }
      ui.log(`sample bank ${code ? "failed " + code : "done"}`);
      if (code === 10) {                         /* playing: stop it, then again */
        const again = await ui.ask("Stop VNL-1 first",
          "VNL-1 is playing. Press PLAY on the FM-1 to stop it, then write the sample bank.",
          [["again", "Write the sample bank"], ["skip", "Skip it"]]);
        if (again === "again") continue;
        ui.say("The firmware is installed; the sample bank was not written (tools/fm1_bank.py writes it).");
        return;
      }
      if (code) throw new Error(`The firmware is installed, but the sample bank was not written: ` +
                                `${ERR[code] || "error " + code}. Try again, or use tools/fm1_bank.py.`);
      ui.progress(100);
      ui.say("Done: VNL-1 and its sample bank are installed.");
      return;
    }
  },
})
