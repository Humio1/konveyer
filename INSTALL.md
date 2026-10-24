# Installing TISHINA (macOS)

1. Double-click **Install TISHINA.command**.
   macOS may say the file is from an unidentified developer. In that case right-click it, choose **Open**, then **Open** again.
2. Restart Ableton Live, or rescan plug-ins in Settings → Plug-Ins.
3. Put TISHINA on a pad, synth or vocal track (or a bus). It adds about 43 ms of latency, which Live compensates automatically.

Manual install: copy `TISHINA.vst3` to `~/Library/Audio/Plug-Ins/VST3` and `TISHINA.component` to
`~/Library/Audio/Plug-Ins/Components`, then run in Terminal:

    xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/TISHINA.vst3 ~/Library/Audio/Plug-Ins/Components/TISHINA.component

This build is not yet signed with an Apple Developer ID, which is why macOS asks.
The fonts inside the plugin are licensed under the SIL Open Font License; see the Licenses folder.
