"""Build the offline tool ZIP, with a self-contained copy of the designer HTML."""
import argparse
from pathlib import Path
from tempfile import TemporaryDirectory
from zipfile import ZIP_DEFLATED, ZipFile


def package(destination):
    source = Path(__file__).resolve().parent
    html = (source / "lvgl_designer.html").read_text(encoding="utf-8")
    module = (source / "designer-taskgen.js").read_text(encoding="utf-8")
    include = '<script src="designer-taskgen.js"></script>'
    if html.count(include) != 1 or "</script" in module.lower():
        raise ValueError("Cannot embed designer-taskgen.js in the designer HTML")
    html = html.replace(include, "<script>\n" + module + "\n</script>")
    assets = ("任务生成器.exe", "new_task.py", "templates/task_template.c", "使用说明.txt")
    with TemporaryDirectory(dir=destination.parent) as scratch:
        temporary = Path(scratch) / "designer.zip"
        with ZipFile(temporary, "w", compression=ZIP_DEFLATED) as archive:
            archive.writestr("lvgl_designer.html", html.encode("utf-8"))
            for name in assets:
                archive.write(source / name, name)
        with ZipFile(temporary) as archive:
            if archive.testzip() is not None:
                raise ValueError("ZIP verification failed")
        temporary.replace(destination)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("destination", type=Path, help="output .zip path")
    args = parser.parse_args()
    if args.destination.suffix.lower() != ".zip":
        parser.error("Expected a .zip path")
    package(args.destination.resolve())
