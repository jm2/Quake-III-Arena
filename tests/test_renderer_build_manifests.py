"""Build manifest checks: portable image decoders (#42/#43) and native libc (#237)."""
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


    def test_native_cmake_build_excludes_qvm_libc_replacement(self):
        """bg_lib.c replaces libc for QVM bytecode only; natively it overrides rand/atof (#237)."""
        cmake = re.sub(r"#[^\n]*", "", (ROOT / "CMakeLists.txt").read_text())
        match = re.search(r"file\(GLOB GAME_SRCS code/game/\*\.c\)\s*"
                          r"list\(REMOVE_ITEM GAME_SRCS([^)]*)\)", cmake)
        self.assertIsNotNone(match)
        self.assertIn("/code/game/bg_lib.c", match.group(1))
        self.assertEqual(cmake.count("bg_lib.c"), 1)        # not re-added anywhere
        self.assertEqual(cmake.count("code/game/*.c"), 1)   # no second glob of the dir

    def test_native_legacy_recipes_list_every_cmake_cgame_source(self):
        """Each native legacy cgame recipe compiles every code/cgame source CMake does (#296).

        cg_particles.c left the legacy recipes out when the particle code moved
        out of cg_marks.c, so none of them linked CG_AddParticles and the rest.
        """
        cmake = re.sub(r"#[^\n]*", "", (ROOT / "CMakeLists.txt").read_text())
        self.assertIn("file(GLOB ALL_CGAME_SRCS code/cgame/*.c)", cmake)
        self.assertRegex(cmake, r"set\(CGAME_MP_SRCS \$\{ALL_CGAME_SRCS\}\)")
        match = re.search(r"set\(CGAME_SRCS \$\{ALL_CGAME_SRCS\}\)\s*"
                          r"list\(REMOVE_ITEM CGAME_SRCS([^)]*)\)", cmake)
        self.assertIsNotNone(match)
        team_arena = {path.name for path in (ROOT / "code/cgame").glob("*.c")}
        removed = {Path(item.strip('"')).name for item in match.group(1).split()}
        self.assertEqual(removed, {"cg_newdraw.c"})
        self.assertIn("cg_particles.c", team_arena)
        base = team_arena - removed

        def make_sources(path, header, prefix, source_dir):
            """code/cgame sources of a make object list with a rule for each object."""
            text = (ROOT / path).read_text()
            block = re.search(re.escape(header) + r"\n((?:\t[^\n]*\n)+)", text)
            self.assertIsNotNone(block, f"{path}: {header}")
            objects = re.findall(re.escape(prefix) + r"(\w+)\.o", block.group(1))
            self.assertTrue(objects, f"{path}: {header}")
            sources = set()
            for stem in objects:
                rule = re.search(r"^" + re.escape(f"{prefix}{stem}.o") + r"\s*:\s*\$\((\w+)\)/"
                                 + re.escape(f"{stem}.c") + ";", text, re.M)
                self.assertIsNotNone(rule, f"{path}: no rule for {prefix}{stem}.o")
                if rule.group(1) == source_dir:
                    sources.add(f"{stem}.c")
            return sources

        conscript = (ROOT / "code/cgame/Conscript").read_text()

        def cons_files(name):
            files = re.search(r"@" + name + r" = qw\((.*?)\);", conscript, re.S)
            self.assertIsNotNone(files, f"Conscript: @{name}")
            return {item for item in files.group(1).split() if "/" not in item}

        cons_native = cons_files("FILES") | cons_files("SO_FILES")

        visual = ET.parse(ROOT / "code/cgame/cgame.vcproj").getroot()
        configurations = [c.attrib["Name"] for c in visual.find("Configurations").iter("Configuration")]
        self.assertEqual(len(configurations), 4)

        def visual_sources(configuration):
            """Sources the Visual Studio project builds in a configuration (lowercased)."""
            built = set()
            for element in visual.iter("File"):
                name = element.attrib["RelativePath"]
                if "\\" in name or not name.endswith(".c"):
                    continue
                if not any(c.attrib.get("Name") == configuration
                           and c.attrib.get("ExcludedFromBuild") == "TRUE"
                           for c in element.iter("FileConfiguration")):
                    built.add(name.lower())
            return built

        xcode = (ROOT / "code/macosx/Quake3.pbproj/project.pbxproj").read_text()
        objects = dict(re.findall(r"\t\t([A-F0-9]{24}) = \{(.*?)\n\t\t\};", xcode, re.S))
        cgame_groups = [body for body in objects.values()
                        if "isa = PBXGroup;" in body and "path = cgame;" in body]
        self.assertEqual(len(cgame_groups), 1)
        targets = [body for body in objects.values()
                   if "isa = PBXBundleTarget;" in body and "name = cgame;" in body]
        self.assertEqual(len(targets), 1)
        xcode_sources = set()
        for phase in re.findall(r"([A-F0-9]{24}),", re.search(r"buildPhases = \((.*?)\);", targets[0], re.S).group(1)):
            if "isa = PBXSourcesBuildPhase;" not in objects[phase]:
                continue
            for build in re.findall(r"([A-F0-9]{24}),", objects[phase]):
                ref = re.search(r"fileRef = ([A-F0-9]{24});", objects[build]).group(1)
                if f"{ref}," in cgame_groups[0]:
                    xcode_sources.add(re.search(r"path = ([^;]+);", objects[ref]).group(1))

        lint = {line[len("cgame\\"):] for line in (ROOT / "code/cgame.lnt").read_text().split()
                if line.startswith("cgame\\")}

        recipes = {
            "code/unix/Makefile baseq3 cgame": (
                make_sources("code/unix/Makefile", "Q3CGOBJ = \\", "$(B)/baseq3/cgame/", "CGDIR"), base),
            "code/unix/Makefile missionpack cgame": (
                make_sources("code/unix/Makefile", "MPCGOBJ = \\", "$(B)/missionpack/cgame/", "CGDIR"), team_arena),
            "code/unix/Makefile q3static": (
                make_sources("code/unix/Makefile", "## add BASEQ3 CGAME\nQ3SOBJ += \\", "$(B)/q3static/", "CGDIR"), base),
            "code/unix/Makefile.Game cgame": (
                make_sources("code/unix/Makefile.Game", "CGOBJ = \\", "$(SRCDIR)/cgame/", "CGDIR"), base),
            "code/cgame/Conscript Q3 .so": (cons_native, base),
            "code/cgame/Conscript TA .so": (cons_native | cons_files("TA_FILES"), team_arena),
            "code/macosx/Quake3.pbproj cgame target": (xcode_sources, team_arena),
            "code/cgame.lnt": (lint, base),
        }
        for configuration in configurations:
            recipes[f"code/cgame/cgame.vcproj {configuration}"] = (
                visual_sources(configuration),
                {name.lower() for name in (team_arena if " TA|" in configuration else base)})
        # Missing since the GPL release, and left alone here: Makefile.Game
        # builds only the QVMs by default, and its .so object list never had
        # the native syscall stubs.
        known_missing = {"code/unix/Makefile.Game cgame": {"cg_syscalls.c"}}

        for recipe, (listed, required) in recipes.items():
            with self.subTest(recipe=recipe):
                missing = required - listed
                self.assertEqual(missing, known_missing.get(recipe, set()))

if __name__ == "__main__":
    unittest.main()
