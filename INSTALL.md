# Installing KONVEYER (macOS)

1. Double-click **Install KONVEYER.command**.
   macOS may say the file is from an unidentified developer. In that case right-click it, choose **Open**, then **Open** again.
2. Restart Ableton Live, or rescan plug-ins in Settings → Plug-Ins.
3. Put KONVEYER last on your master channel.

Manual install: copy `KONVEYER.vst3` to `~/Library/Audio/Plug-Ins/VST3` and `KONVEYER.component` to
`~/Library/Audio/Plug-Ins/Components`, then run in Terminal:

    xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/KONVEYER.vst3 ~/Library/Audio/Plug-Ins/Components/KONVEYER.component

This build is not yet signed with an Apple Developer ID, which is why macOS asks.
The fonts inside the plugin are licensed under the SIL Open Font License; see the Licenses folder.
