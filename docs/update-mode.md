# Update mode

The D878UVII's boot loader (the first 16 KB of MCU flash) can rewrite the
firmware image above it. `dmm-radio flash` speaks its protocol, so a
radio can be flashed from Linux, macOS or Windows without the vendor's
programming software. This page describes that protocol as observed on
the USB link of a D878UVII PLUS whose boot loader reports version `V100`.

## Entering update mode

Switch the radio off. Hold **PTT and PF3** (the top key) and switch it on.
The display stays dark and the red LED blinks. The radio shows up as the
same USB serial device as in normal use (USB IDs 28e9:018a).

PTT and PF1 at power-on is a different start-up: the factory firmware asks
"Initialize the radio?". Press Exit there.

## Using `dmm-radio flash`

```sh
dmm-radio flash <file.CDD>
```

The `.spi` and `.CDI` files must sit next to the `.CDD` file with the same
name. Any D878UVII firmware set works: DeltaMM's update files from
`dmm-radio prepare` (or the `update-files` build target), or the factory
files, to go back to the factory firmware.

Before sending anything, the tool checks that:

- a verified backup of the radio exists (the same setting as for
  `prepare`, `dmm-radio config backup-before-flash`);
- the `.spi` header names the D878UVII, the `.CDI` header loads the image
  at `0x08004000`, and both give the `.CDD` file's length;
- the image starts with a stack pointer in RAM and a reset handler inside
  the image, and ends below the radio data at `0x080F5800`;
- the radio answers in update mode and reports the model `D878UV2`.

A transfer takes about 9 s for DeltaMM (100 KB) and about 80 s for the
factory firmware (900 KB). When it ends, the radio starts the new firmware
by itself.

If the transfer stops, the tool names the address where it stopped. The
boot loader is never written, so the radio stays in update mode: run
`flash` again with the same files or with the factory ones.

## Protocol

The link is USB CDC serial; baud rate and control lines make no
difference. Every request waits for its reply before the next one goes
out.

| Step | PC sends | Radio answers |
| --- | --- | --- |
| Start | `55 50 44 41 54 45` ("UPDATE") | `06` |
| Identify | `02` | 16 bytes, see below |
| Write | one 40-byte frame per 32 image bytes | `06` per frame |
| Finish | `18` | `06`, then the radio starts the new image |

### Identify reply

| Offset | Size | Content |
| --- | --- | --- |
| 0 | 1 | `49` ("I") |
| 1 | 8 | model, ASCII, zero-padded: "D878UV2" |
| 9 | 6 | boot loader version, ASCII, zero-padded: "V100" |
| 15 | 1 | `06` |

### Write frame

| Offset | Size | Content |
| --- | --- | --- |
| 0 | 1 | `01` |
| 1 | 4 | flash address, little-endian |
| 5 | 32 | image bytes; the last frame is padded with `00` |
| 37 | 2 | sum of bytes 1 to 36 (address and data) as a 16-bit number, little-endian |
| 39 | 1 | `06` |

Frames go out in address order from the image's load address,
`0x08004000`, in steps of 32 bytes. The first frame of each 1 KB flash page
takes about 80 ms to be acknowledged instead of about 1 ms: the boot loader
erases the page when that frame arrives.

The `.spi` and `.CDI` files are not sent. They only name the model, the
load address and the image length.

### Not known

- The radio's answer to a bad checksum or a frame it rejects. The tool
  treats anything other than `06`, or no answer within 2 s, as a failure
  and stops.
- Whether frames out of order, repeated, or outside the image area are
  accepted. The tool never sends them.
