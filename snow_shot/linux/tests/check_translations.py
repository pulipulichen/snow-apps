"""Qt 6.4 lacks lrelease -fail-on-unfinished; enforce completeness before building."""
from pathlib import Path
import re
import xml.etree.ElementTree as ET

root = Path(__file__).resolve().parents[1]
expected = {}
for filename in (root / "src").glob("*.cpp"):
    content = filename.read_text()
    if filename.name == "main.cpp":
        expected["Launcher"] = set(re.findall(r'(?:Launcher::)?tr\("([^"\n]*)"\)', content))
    elif filename.name == "widgets.cpp":
        boundary = content.index("Canvas::Canvas")
        expected["SelectionOverlay"] = set(re.findall(r'tr\("([^"\n]*)"\)', content[:boundary]))
        expected["Editor"] = set(re.findall(r'(?:Editor::)?tr\("([^"\n]*)"\)', content[boundary:]))

for locale in ("en_US", "zh_CN", "zh_TW"):
    path = root / "i18n" / f"snow_shot_linux_{locale}.ts"
    catalog = ET.parse(path).getroot()
    actual = {}
    for context in catalog.findall("context"):
        name = context.findtext("name")
        actual[name] = set()
        for message in context.findall("message"):
            source = message.findtext("source")
            translation = message.find("translation")
            assert translation is not None and translation.get("type") is None, (path, source)
            text = "".join(translation.itertext())
            assert text.strip(), (path, source)
            assert sorted(re.findall(r"%[1-9n]", source)) == sorted(re.findall(r"%[1-9n]", text))
            actual[name].add(source)
    assert actual == expected, (path, actual, expected)
print("All Linux translation catalogs are complete.")
