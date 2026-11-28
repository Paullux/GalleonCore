# SPDX-FileCopyrightText: 2026 Paul Woisard
# SPDX-License-Identifier: GPL-3.0-or-later
"""The KDE plasmoid of the project is a well formed Plasma package, and it keeps the widgets shared.

Plasma is not needed: this reads the files. What the plasmoid does with the widget is in tests/test_plasmoid.cpp."""
import json
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def problems(folder: Path, metadata: dict) -> list[str]:
    """What is wrong with a plasmoid folder and its metadata, as messages. Pure but for the files it looks at."""
    found = []
    plugin = metadata.get("KPlugin", {})
    if plugin.get("Id") != folder.name:
        found.append(f"KPlugin.Id is {plugin.get('Id')!r}, it must be the name of the folder, {folder.name!r}")
    if metadata.get("KPackageStructure") != "Plasma/Applet":
        found.append("KPackageStructure must be Plasma/Applet")
    if not re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", str(plugin.get("Version", ""))):
        found.append("KPlugin.Version must be like 1.2.3")
    if plugin.get("License") != "GPL-3.0-or-later":
        found.append("KPlugin.License must be GPL-3.0-or-later, the licence of the project outside lib/")
    if not plugin.get("Name") or not plugin.get("Description"):
        found.append("KPlugin.Name and KPlugin.Description are needed")
    if not (folder / "contents" / "ui" / "main.qml").is_file():
        found.append("contents/ui/main.qml is missing")
    return found


class PlasmoidPackageTests(unittest.TestCase):
    def test_every_plasmoid_of_the_project_is_a_well_formed_package(self):
        folders = sorted(path for path in (ROOT / "plasmoid").iterdir() if path.is_dir())
        self.assertTrue(folders)
        for folder in folders:
            metadata = json.loads((folder / "metadata.json").read_text(encoding="utf-8"))
            self.assertEqual(problems(folder, metadata), [], folder.name)

    def test_a_plasmoid_does_not_keep_a_copy_of_a_shared_widget(self):
        shared = {path.name for path in (ROOT / "widgets").glob("*.qml")}
        for qml in (ROOT / "plasmoid").rglob("*.qml"):
            self.assertNotIn(qml.name, shared, f"{qml}: widgets/ is the one copy, the plasmoid loads it")

    def test_problems_names_what_is_wrong(self):
        folder = ROOT / "plasmoid" / "org.galleoncore.clock"
        good = {"KPlugin": {"Id": folder.name, "Version": "0.1.0", "License": "GPL-3.0-or-later", "Name": "n", "Description": "d"},
                "KPackageStructure": "Plasma/Applet"}
        self.assertEqual(problems(folder, good), [])
        wrong_id = {**good, "KPlugin": {**good["KPlugin"], "Id": "other"}}
        self.assertEqual(len(problems(folder, wrong_id)), 1)
        self.assertEqual(len(problems(folder, {**good, "KPackageStructure": "Plasma/Wallpaper"})), 1)
        self.assertEqual(len(problems(folder, {**good, "KPlugin": {**good["KPlugin"], "Version": "one"}})), 1)
        self.assertEqual(len(problems(folder, {**good, "KPlugin": {**good["KPlugin"], "License": "MIT"}})), 1)
        self.assertEqual(len(problems(folder, {"KPackageStructure": "Plasma/Applet"})), 4)
        self.assertEqual(len(problems(ROOT / "plasmoid" / "nowhere", good)), 2)  # the Id differs, and there is no main.qml


if __name__ == "__main__":
    unittest.main()
