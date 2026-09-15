"""Independent test saves; APFS clones share disk blocks until either copy changes."""
from pathlib import Path
import shutil
import subprocess
import sys


def copy_save(source, destination):
    destination = Path(destination)
    if destination.exists() or destination.is_symlink():
        raise FileExistsError(destination)
    if sys.platform == "darwin":
        result = subprocess.run(["/bin/cp", "-c", "-p", str(source), str(destination)],
                                capture_output=True)
        if result.returncode == 0:
            return
        # A failed clone may leave a partial destination; this call created it.
        destination.unlink(missing_ok=True)
    shutil.copy2(source, destination)


if __name__ == "__main__":
    import tempfile
    with tempfile.TemporaryDirectory() as directory:
        source, copy = (Path(directory) / name for name in ("source.sav", "copy.sav"))
        source.write_bytes(b"original save" * 4096)
        copy_save(source, copy)
        assert source.read_bytes() == copy.read_bytes()
        assert source.stat().st_ino != copy.stat().st_ino
        with copy.open("r+b") as stream:
            stream.write(b"modified")
        assert source.read_bytes() == b"original save" * 4096
        try:
            copy_save(source, copy)
        except FileExistsError:
            pass
        else:
            raise AssertionError("Overwrote an existing destination")
    print("PASS independent save copy and overwrite protection")
