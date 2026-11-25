"""Add the developer setup commands to iSH's bundled Alpine filesystem."""
from pathlib import Path
import io
import sys
import tarfile

archive = Path(sys.argv[1])
overlay = Path(__file__).parent / 'overlay'
output = archive.with_suffix('.new.gz')
with tarfile.open(archive, 'r:gz') as original, tarfile.open(output, 'w:gz') as updated:
    for item in original:
        updated.addfile(item, original.extractfile(item) if item.isfile() else None)
    for local, destination, mode in [
        ('setup-dev', './usr/local/bin/setup-dev', 0o755),
        ('pkg', './usr/local/bin/pkg', 0o755),
        ('terminal32-profile.sh', './etc/profile.d/terminal32.sh', 0o644),
    ]:
        data = (overlay / local).read_bytes()
        item = tarfile.TarInfo(destination)
        item.mode = mode
        item.size = len(data)
        updated.addfile(item, io.BytesIO(data))
output.replace(archive)
