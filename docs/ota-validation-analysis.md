# OTA image validation analysis

This is analysis only. The OTA fixes do not introduce firmware format or
compatibility validation.

The existing token controls who can upload. The caller-supplied SHA-256 checks
that the stored bytes match the caller's file. Neither detects an accidental
upload of the wrong file when its checksum was calculated correctly. This is
still useful to address on a private WiFi network, as an operator-error concern.

## Checks possible without changing the firmware artifact

The current PlatformIO output is a raw binary flashed at `0x10000000`.
The locally installed framework places the application vector table at binary
offset `0x3000`; the preceding bytes contain boot2 and the OTA bootloader.
These offsets are framework-specific, and should be treated as a supported
image layout rather than a universal RP2040 format.

1. **Bound the image.** Require enough bytes for the bootloader, layout metadata,
   and application vector table. Bound the destination by the running firmware's
   `_FS_start` linker symbol, allowing for PicoOTA's writes in complete 4096-byte
   sectors. This protects the filesystem and staged image from overlap. The
   current release layout starts LittleFS at `0x1033f000`, leaving `0x33f000`
   bytes before it. Also check available LittleFS space for staging and command
   metadata. File length alone does not establish compatibility.
2. **Verify boot2's checksum.** RP2040 boot2 occupies the first 256 bytes;
   its last four bytes contain the boot ROM's CRC of the preceding 252 bytes.
   Reproduce the Pico SDK's `boot_stage2/pad_checksum` algorithm, including its
   bit ordering and seed. This catches corruption in boot-critical bytes and
   rejects many unrelated files. It does not establish board identity: another
   RP2040 binary can also have a valid boot2 checksum.
3. **Check the application vector table.** Read the initial stack pointer and
   reset vector at `0x3000`. Require an appropriately aligned stack pointer in
   the RP2040 SRAM region (allowing a stack top at the end of RAM), and a reset
   vector with the Thumb bit set whose address falls within the uploaded
   application region. This catches empty images, obvious truncation, and many
   wrong formats. A plausible vector table does not prove all code is present.
4. **Check filesystem layout metadata.** The local OTA bootloader reads the
   filesystem start/end addresses stored at offsets `0x2ff0` and `0x2ff4`.
   Compare the incoming values with the running firmware's `_FS_start` and
   `_FS_end`. Refuse layout changes through normal OTA, since changing these
   addresses can hide the staged image, command file, or application data from
   the next bootloader. Intentional partition changes need a separate migration
   procedure.
5. **Recognize unsupported containers.** Reject ELF and UF2 magic values with a
   useful error asking for `firmware.bin`. PicoOTA also recognizes gzip: either
   explicitly support it with bounded decompression and validation of the
   decompressed image, or reject it in a validator that supports only raw `.bin`.
   Applying raw-file length and vector checks to gzip bytes would be incorrect.

All these checks can stream through the filesystem task with a small buffer;
the board does not need to allocate an image-sized block of RAM. Check the
complete image before committing an OTA command.

## Stronger compatibility checks require build metadata

A build-generated manifest could declare a product identifier, supported MCU,
board family, image length, image format version, partition layout, and digest.
That would distinguish this project's firmware from another otherwise plausible
RP2040 image. Use a family identifier rather than the physical board UID: this
project intentionally shares firmware between boards selected by UID at runtime.
An explicit length and build-generated digest also help detect a truncated
artifact that was subsequently rehashed by the upload script.

A signature would establish that a trusted signing key approved the artifact.
It is a separate decision from the structural checks and requires key management.
Neither a manifest nor a signature guarantees correct runtime behavior.

For the stated local-access use case, the practical first step would be raw
binary bounds, boot2 CRC, application vectors, and matching filesystem layout.
A manifest can be added later if distinguishing product builds becomes useful.
Power loss during in-place flashing and firmware bugs require recovery or
rollback mechanisms beyond pre-upload validation; the current PicoOTA flow does
not provide an A/B fallback image.
