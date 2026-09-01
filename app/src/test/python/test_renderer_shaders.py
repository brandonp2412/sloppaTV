import pathlib
import unittest


class RendererShaderSourceTest(unittest.TestCase):
    def test_version_directive_is_first_shader_byte(self):
        source = pathlib.Path("app/src/main/cpp/renderer.cpp").read_text()
        self.assertNotIn('R"(\n#version', source)


if __name__ == "__main__":
    unittest.main()
