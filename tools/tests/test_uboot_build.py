# SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
import importlib.util
from pathlib import Path
import pytest

MODULE_PATH = Path(__file__).resolve().parents[2] / "examples/uboot/build.py"
spec = importlib.util.spec_from_file_location("uboot_example_build", MODULE_PATH)
build = importlib.util.module_from_spec(spec)
spec.loader.exec_module(build)


def test_stage_refuses_existing_destination(tmp_path):
    stage = tmp_path / "existing"
    stage.mkdir()
    marker = stage / "owner.txt"
    marker.write_text("preserve")
    with pytest.raises(FileExistsError):
        build.stage_source(tmp_path / "not-a-repo", stage)
    assert marker.read_text() == "preserve"


def test_stage_refuses_source_missing_pinned_commit(tmp_path):
    stage = tmp_path / "new"
    with pytest.raises(ValueError, match="pinned U-Boot commit"):
        build.stage_source(tmp_path / "missing-source", stage)
    assert not stage.exists()


def test_stage_refuses_writing_inside_source_checkout(tmp_path):
    source = tmp_path / "source"
    source.mkdir()
    stage = source / "new-stage"
    with pytest.raises(ValueError, match="outside"):
        build.stage_source(source, stage)
    assert not stage.exists()
