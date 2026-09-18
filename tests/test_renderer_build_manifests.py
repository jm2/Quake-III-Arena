"""Issues #42/#43: keep portable image decoders linked in every existing renderer target."""
from pathlib import Path
import re
import unittest
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]


class RendererBuildManifests(unittest.TestCase):
    def test_image_decoders_follow_original_module_in_every_target(self):
        """A decoder call must not become an undefined symbol outside the CMake glob."""
        makefile = (ROOT / "code/unix/Makefile").read_text()
        cons = (ROOT / "code/unix/Conscript-client").read_text()
        lint = (ROOT / "code/renderer.lnt").read_text()
        visual = ET.parse(ROOT / "code/renderer/renderer.vcproj").getroot()
        visual_sources = {f.attrib.get("RelativePath") for f in visual.iter("File")}
        xcode = (ROOT / "code/macosx/Quake3.pbproj/project.pbxproj").read_text()
        objects = dict(re.findall(r"\t\t([A-F0-9]{24}) = \{(.*?)\n\t\t\};", xcode, re.S))

        def reference(name):
            matches = [key for key, body in objects.items()
                       if "isa = PBXFileReference;" in body and f"path = {name};" in body]
            self.assertEqual(len(matches), 1, name)
            return matches[0]

        original = reference("tr_image.c")
        source_phases = [body for body in objects.values() if "isa = PBXSourcesBuildPhase;" in body]
        original_builds = {key for key, body in objects.items()
                           if "isa = PBXBuildFile;" in body and f"fileRef = {original};" in body}
        required_phases = [body for body in source_phases
                           if any(f"{key}," in body for key in original_builds)]
        self.assertEqual(len(required_phases), len(original_builds))
        self.assertGreater(len(required_phases), 0)

        self.assertNotIn("path = jload.c;", xcode)
        self.assertFalse((ROOT / "code/jpeg-6/jload.c").exists())
        image_modules = sorted((ROOT / "code/renderer").glob("tr_image_*.c"))
        for path in image_modules + [ROOT / "code/jpeg-6/jcapistd.c"]:
            with self.subTest(decoder=path.name):
                for directory in ("client", "q3static"):
                    self.assertIn(f"$(B)/{directory}/{path.stem}.o \\", makefile)
                    self.assertRegex(makefile, re.escape(f"$(B)/{directory}/{path.stem}.o")
                                     + r"\s*:\s*" + re.escape(f"$({'RDIR' if path.parent.name == 'renderer' else 'JPDIR'})/{path.name}"))
                self.assertIn(f"../{path.parent.name}/{path.name}", cons)
                self.assertIn(path.name if path.parent.name == "renderer" else f"..\\jpeg-6\\{path.name}", visual_sources)
                if path.parent.name == "renderer":
                    self.assertIn(f"renderer\\{path.name}", lint)
                ref = reference(path.name)
                decoder_builds = {key for key, body in objects.items()
                                  if "isa = PBXBuildFile;" in body and f"fileRef = {ref};" in body}
                self.assertEqual(len(decoder_builds), len(original_builds))
                for phase in required_phases:
                    self.assertEqual(sum(f"{key}," in phase for key in decoder_builds), 1)
                self.assertTrue(any("isa = PBXGroup;" in body and f"{ref}," in body
                                    for body in objects.values()))


if __name__ == "__main__":
    unittest.main()
