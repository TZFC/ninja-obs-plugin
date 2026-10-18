# Independent guest return validation

The Guest Return tab owns a separate `vdoninja_output`, service, H.264 encoder, and Opus encoder. It uses OBS's program video and binds the selected audio mixer to encoded slot zero. It never replaces OBS's main stream service or borrows its encoders. Remote control and automatic inbound-source creation remain disabled on this output.

The return stops before profile or scene-collection changes and during dock shutdown. Hiding the dock does not stop it. A stop requested before signaling connects also completes libobs's stop sequence; otherwise releasing a connecting output can wait indefinitely.

## Automated checks

The normal tests-only build includes `vdoninja-guest-return-tests`. Its OBS stubs exercise mixer tracks 1–6, encoder ownership, cancellation before capture, repeated stop, active-session identity, synchronous resource failures, asynchronous errors and restart, invalid configuration, and clearing errors for a new profile. The stub API deliberately provides no frontend streaming controls. All 759 tests passed; changed C++ files passed clang-format 14 checks.

```sh
cmake --build build-installer-release-tests --config Release --target vdoninja-tests
ctest --test-dir build-installer-release-tests -C Release --output-on-failure
```

## Windows runtime check, 2026-10-18

Used an isolated portable OBS 32.2.2 instance and verified that its loaded module path pointed to the locally built DLL. Tested x264 and NVIDIA NVENC H.264, then repeated the NVENC run using a clean checkout of the committed guest-return change. The normal stream used H.264/AAC on track 1 and sent to a local RTMP receiver; no public streaming account was used. A real Chromium VDO.Ninja guest published synthetic camera/microphone media and received the return in the same page.

Four distinct tones represented the host (300 Hz), guest (600 Hz), music (900 Hz), and clip (1200 Hz). The guest tone entered OBS through a VDO.Ninja Source; the other tones were OBS media sources.

- Including all four sources on return track 2 made all four tones measurable at the guest.
- Excluding only the guest from track 2 reduced its returned tone by approximately 50 dB in the clean-checkout run. The other tones changed by less than 1 dB. The local RTMP recording still contained the guest.
- Re-including the guest restored its tone. Muting and restoring the clip affected the return as expected.
- Full program video decoded at the guest while the main stream ran. Either output could start first.
- Stopping the return left the main stream and recording active. Restarting the return reconnected the guest; stopping the main stream left the return playing.
- Hiding the dock left the return active. Scene-collection and profile changes released it. Profile-specific identities and the main stream service were preserved.
- Five quick start/stop cycles completed, the copied viewer link matched the active return, and OBS exited while the return was active without hanging.

The Guest Return tab was inspected at a narrow dock width with its settings disabled while running. A deliberately missing encoder showed an error without creating an output; selecting x264 allowed a successful retry. Switching profiles cleared the prior error.

These checks cover one manually configured return mix on Windows. macOS/Linux runtime behavior and separate mixes for multiple guests are outside this validation. The feature does not change the release SDK baseline or add automatic source routing.
