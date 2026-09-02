// SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
//! JEEFS EEPROM header parsing library.
//!
//! Provides zero-copy parsing and CRC32 verification of JEEFS EEPROM headers
//! (versions 1, 2, and 3). `no_std` compatible — no heap allocation required.

#![no_std]

#[cfg(feature = "alloc")]
extern crate alloc;
#[cfg(feature = "std")]
extern crate std;

pub mod devid;
pub mod generated;
pub mod header;
#[cfg(feature = "alloc")]
pub mod image;

pub use devid::*;
pub use generated::*;
pub use header::*;
#[cfg(feature = "alloc")]
pub use image::*;
