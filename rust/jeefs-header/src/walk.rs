// SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
//! Pull-model file locator — the Rust port of `src/jeefs_walk.c` (#109).
//!
//! Finds one file in the JEEFS chain without buffering the image. The
//! caller owns every read; this module owns the state machine and applies
//! the in-memory iterator's validation rules to each header it visits —
//! `headerCrc32` before any field is trusted, exact contiguity, bounds,
//! and the erased-link and empty-slot terminals. Validation runs up to and
//! including the match and stops there: unlike [`crate::fs::read_file`],
//! the walker does not certify the rest of the chain. [`Step::Found`]
//! certifies the prefix it walked; the payload is certified separately by
//! [`DataVerifier`].
//!
//! Peak RAM is this struct plus one 28-byte header window — the reason a
//! firmware too small to hold its EEPROM can still read a file:
//!
//! ```no_run
//! # use jeefs_header::walk::{DataVerifier, Step, Walk};
//! # fn env_read(_off: u32, _buf: &mut [u8]) {}
//! # let prefix = [0u8; 256];
//! # let image_size = 8192u16;
//! let mut w = Walk::begin(&prefix, image_size, "wifi.conf")?;
//! let mut hdr = [0u8; 28];
//!
//! let found = loop {
//!     match w.step() {
//!         Step::Want { offset, len } => {
//!             env_read(offset, &mut hdr[..len as usize]);
//!             w.feed(&hdr[..len as usize])?;
//!         }
//!         Step::Found(f) => break Some(f),
//!         Step::NotFound => break None,
//!         Step::Failed(e) => return Err(e),
//!     }
//! };
//!
//! if let Some(f) = found {
//!     let mut v = DataVerifier::new(&f);
//!     // stream the payload from f.offset() in windows
//!     # let chunk = [0u8; 4];
//!     v.update(&chunk);
//!     assert!(v.finish());
//! }
//! # Ok::<(), jeefs_header::fs::FsError>(())
//! ```
//!
//! `prefix` is the board-header prefix the environment has already read
//! (at least 12 bytes: enough for version detection and the `fs_version`
//! byte). Verifying the board-header CRC first stays the caller's job,
//! exactly as it is with [`crate::header::verify_crc`].

use crate::fs::{byte_is_empty, crc32, filename_valid, FsError, FHDR, FHDR_CRC_COVERAGE};
use crate::generated::{FILE_NAME_LENGTH, FS_VERSION, FS_VERSION_OFFSET};
use crate::header::{detect_version, header_size};

/// Bytes of board header needed to detect a version and read `fs_version`
/// (`JEEPROMHeaderversion` on the wire).
const VERSION_PROBE: usize = 12;

/// A located file: where its payload starts, how long it is, and the CRC32
/// the payload must have.
///
/// Produced only by a walk that reached [`Step::Found`]: the fields are
/// private and there is no constructor, so a `Found` cannot be assembled
/// by hand and handed to [`DataVerifier`] with a CRC the chain never
/// recorded.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Found {
    offset: u32,
    size: u16,
    crc32: u32,
}

impl Found {
    /// Offset of the first payload byte in the image.
    pub fn offset(&self) -> u32 {
        self.offset
    }

    /// Payload length in bytes.
    pub fn size(&self) -> u16 {
        self.size
    }

    /// Expected CRC32 of the payload, as the chain recorded it.
    pub fn crc32(&self) -> u32 {
        self.crc32
    }
}

/// What the walker needs next, or how the walk ended.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Step {
    /// The environment must read `len` bytes at `offset` and hand them to
    /// [`Walk::feed`]. `len` is always the file-header width.
    Want { offset: u32, len: u16 },
    /// The file was located.
    Found(Found),
    /// The chain ended without a match.
    NotFound,
    /// Validation failed and the walk is over. The same error [`Walk::feed`]
    /// returned; kept here so a loop driven by [`Walk::step`] terminates
    /// instead of asking for the window that just failed.
    Failed(FsError),
}

/// Internal state. The public shape is [`Step`]; this mirrors the state
/// variable of the C walker so both reach the same terminal on any image.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum State {
    Wanting(u32),
    Found(Found),
    NotFound,
    Failed(FsError),
}

/// A walk in progress. See the [module documentation](self).
#[derive(Debug, Clone)]
pub struct Walk {
    state: State,
    image_size: u16,
    target: [u8; FILE_NAME_LENGTH + 1],
}

impl Walk {
    /// Start a walk over an image of `image_size` bytes, looking for
    /// `filename`.
    ///
    /// `prefix` is the board-header prefix the environment already holds
    /// and must be at least 12 bytes.
    ///
    /// Two conditions succeed and terminate as [`Step::NotFound`] without
    /// asking for a single byte: an `fs_version` of 0 (no filesystem), and
    /// an image with no room for a file header after the board header.
    ///
    /// # Errors
    /// [`FsError::BufferNotValid`] for a short prefix,
    /// [`FsError::FileNameNotValid`] for a name outside the domain
    /// filesystem-v1.md defines, [`FsError::EepromCorrupted`] when no
    /// header is detected or it does not fit `image_size`, and
    /// [`FsError::FsVersionNotSupported`] for an unknown layout.
    pub fn begin(prefix: &[u8], image_size: u16, filename: &str) -> Result<Self, FsError> {
        if prefix.len() < VERSION_PROBE {
            return Err(FsError::BufferNotValid);
        }
        // The target is caller-supplied, so it faces the same name domain
        // as the FS operations (#116).
        if !filename_valid(filename) {
            return Err(FsError::FileNameNotValid);
        }

        let version = detect_version(prefix).ok_or(FsError::EepromCorrupted)?;
        let hdr_size = header_size(version).ok_or(FsError::EepromCorrupted)?;
        if hdr_size > image_size as usize {
            return Err(FsError::EepromCorrupted);
        }

        let fs_version = prefix[FS_VERSION_OFFSET];
        if fs_version != 0 && fs_version as usize != FS_VERSION {
            return Err(FsError::FsVersionNotSupported);
        }

        let mut target = [0u8; FILE_NAME_LENGTH + 1];
        target[..filename.len()].copy_from_slice(filename.as_bytes());

        // No filesystem, or no room for even one file header: a clean end
        // rather than a request the environment cannot satisfy.
        let state = if fs_version == 0 || hdr_size + FHDR > image_size as usize {
            State::NotFound
        } else {
            State::Wanting(hdr_size as u32)
        };

        Ok(Walk {
            state,
            image_size,
            target,
        })
    }

    /// Record a validation failure as the terminal state and hand the
    /// error back. The C walker stores the error in the same variable it
    /// stores its terminals in, so `want` stops asking; without this a
    /// loop driven by `step` would ask for the failing window forever.
    fn fail(&mut self, e: FsError) -> FsError {
        self.state = State::Failed(e);
        e
    }

    /// What the walker needs next, or how the walk ended.
    pub fn step(&self) -> Step {
        match self.state {
            State::Wanting(offset) => Step::Want {
                offset,
                len: FHDR as u16,
            },
            State::Found(f) => Step::Found(f),
            State::NotFound => Step::NotFound,
            State::Failed(e) => Step::Failed(e),
        }
    }

    /// Feed exactly the window [`Step::Want`] asked for.
    ///
    /// Feeding a walker that has already reached a terminal state does
    /// nothing, mirroring the C walker so both stay in step on the same
    /// input.
    ///
    /// # Errors
    /// [`FsError::BufferNotValid`] for a window of the wrong length, and
    /// [`FsError::EepromCorrupted`] for a header that fails validation.
    pub fn feed(&mut self, header: &[u8]) -> Result<(), FsError> {
        let at = match self.state {
            State::Wanting(offset) => offset,
            // Already terminal: idempotent, and a failed walk keeps
            // reporting its failure, exactly as the C walker's stored
            // state does.
            State::Failed(e) => return Err(e),
            _ => return Ok(()),
        };
        if header.len() != FHDR {
            return Err(self.fail(FsError::BufferNotValid));
        }

        // An unwritten slot, in either emptiness domain, ends the chain.
        if byte_is_empty(header[0]) {
            self.state = State::NotFound;
            return Ok(());
        }

        // A written header must checksum before any of its fields is read.
        let stored = u32::from_le_bytes([header[24], header[25], header[26], header[27]]);
        if crc32(&header[..FHDR_CRC_COVERAGE]) != stored {
            return Err(self.fail(FsError::EepromCorrupted));
        }

        // Defense in depth behind the CRC: a terminated name, a sane size.
        if header[FILE_NAME_LENGTH] != 0 {
            return Err(self.fail(FsError::EepromCorrupted));
        }
        let data_size = u16::from_le_bytes([header[16], header[17]]);
        if data_size == 0 || data_size == 0xFFFF {
            return Err(self.fail(FsError::EepromCorrupted));
        }

        let end = at + FHDR as u32 + data_size as u32;
        if end > self.image_size as u32 {
            return Err(self.fail(FsError::EepromCorrupted));
        }

        // An erased link terminates the chain like 0 (RFC #14).
        let mut next = u16::from_le_bytes([header[22], header[23]]);
        if next == 0xFFFF {
            next = 0;
        }
        // Contiguity: the link either terminates or names exactly the next
        // slot, and a claimed successor must have room for its own header.
        if next != 0 && (next as u32 != end || end + FHDR as u32 > self.image_size as u32) {
            return Err(self.fail(FsError::EepromCorrupted));
        }

        if name_matches(header, &self.target) {
            self.state = State::Found(Found {
                offset: at + FHDR as u32,
                size: data_size,
                crc32: u32::from_le_bytes([header[18], header[19], header[20], header[21]]),
            });
            return Ok(());
        }

        self.state = if next == 0 {
            State::NotFound
        } else {
            State::Wanting(next as u32)
        };
        Ok(())
    }
}

/// Compare a stored name field against the target the way the C walker's
/// `strncmp` does: byte by byte, stopping at the first terminator common
/// to both. A full-width comparison would disagree with C on a name whose
/// padding after the terminator is not zero.
fn name_matches(header: &[u8], target: &[u8]) -> bool {
    for i in 0..=FILE_NAME_LENGTH {
        if header[i] != target[i] {
            return false;
        }
        if header[i] == 0 {
            return true;
        }
    }
    true
}

/// Running CRC32 over a payload streamed in windows — the Rust form of
/// `jeefs_crc32_update`, for a target that cannot hold the whole file.
///
/// Obtained from a [`Found`], so there is no way to verify against a CRC
/// the walk did not actually produce.
#[derive(Debug, Clone)]
pub struct DataVerifier {
    hasher: crc32fast::Hasher,
    expect: u32,
    remaining: u32,
    overrun: bool,
}

impl DataVerifier {
    /// Start verifying the payload of a located file.
    pub fn new(found: &Found) -> Self {
        DataVerifier {
            hasher: crc32fast::Hasher::new(),
            expect: found.crc32(),
            remaining: found.size() as u32,
            overrun: false,
        }
    }

    /// Add the next window of payload bytes.
    pub fn update(&mut self, chunk: &[u8]) {
        if chunk.len() as u64 > self.remaining as u64 {
            self.overrun = true;
            self.remaining = 0;
        } else {
            self.remaining -= chunk.len() as u32;
        }
        self.hasher.update(chunk);
    }

    /// True when exactly the file's bytes were fed and they carry the CRC
    /// the chain recorded. A short or overlong stream fails even if the
    /// bytes seen so far happened to checksum — otherwise a truncated read
    /// could pass for a verified file.
    pub fn finish(self) -> bool {
        !self.overrun && self.remaining == 0 && self.hasher.finalize() == self.expect
    }
}
