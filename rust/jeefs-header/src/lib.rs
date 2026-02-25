// SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
//! JEEFS EEPROM header parsing library.
//!
//! Provides zero-copy parsing and CRC32 verification of JEEFS EEPROM headers
//! (versions 1, 2, and 3). `no_std` compatible — no heap allocation required.

#![no_std]

pub mod generated;
pub mod header;

pub use generated::*;
pub use header::*;
