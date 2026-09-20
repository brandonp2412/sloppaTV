import pathlib
import unittest


class RendererShaderSourceTest(unittest.TestCase):
    def test_version_directive_is_first_shader_byte(self) -> None:
        source = pathlib.Path("app/src/main/cpp/renderer_shaders.hpp").read_text()
        self.assertNotIn('R"(\n#version', source)
        self.assertEqual(source.count('R"(#version 300 es'), 6)

    def test_renderer_keeps_shader_source_out_of_implementation(self) -> None:
        source = pathlib.Path("app/src/main/cpp/renderer.cpp").read_text()
        self.assertNotIn("#version 300 es", source)


if __name__ == "__main__":
    unittest.main()
