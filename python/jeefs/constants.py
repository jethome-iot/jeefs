"""EEPROM header constants and signature algorithm definitions.

Re-exports all constants from the auto-generated constants_generated module.
The generated module is the single source of truth — produced by:
    python -m jeefs_codegen --specs docs/format/*.md --py-output python/jeefs/constants_generated.py
"""

from .constants_generated import *  # noqa: F401,F403
from .constants_generated import SignatureAlgorithm  # explicit for type checkers
