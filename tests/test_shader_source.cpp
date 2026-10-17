// Regression tests for the GL shader #version swap.
//
// Bug this guards: shader sources start with a leading newline, and the old
// swap replaced everything up to the *first* newline — leaving the original
// "#version 330 core" line in place and duplicating the directive. Every
// shader then failed to compile, and the OpenGL backend rendered nothing.
#include "test_main.hpp"

#include "shader_source.hpp"

using ui::shader_detail::SwapShaderVersion;

TEST(shader_version_swap_replaces_first_line) {
    const char* src = "\n#version 330 core\nlayout(location = 0) in vec2 aPos;\nvoid main() {}\n";
    const std::string out = SwapShaderVersion(src, "#version 150");
    CHECK_EQ(out, std::string("#version 150\nlayout(location = 0) in vec2 aPos;\nvoid main() {}\n"));
    CHECK(out.find("#version 330 core") == std::string::npos);
}

TEST(shader_version_swap_handles_no_leading_newline) {
    const char* src = "#version 330 core\nvoid main() {}\n";
    const std::string out = SwapShaderVersion(src, "#version 330");
    CHECK_EQ(out, std::string("#version 330\nvoid main() {}\n"));
}

TEST(shader_version_swap_handles_crlf) {
    const char* src = "\r\n#version 330 core\r\nvoid main() {}\r\n";
    const std::string out = SwapShaderVersion(src, "#version 150");
    CHECK(out.rfind("#version 150", 0) == 0);
    CHECK(out.find("#version 330 core") == std::string::npos);
    CHECK(out.find("void main() {}") != std::string::npos);
}

TEST(shader_version_swap_leaves_single_line_source_untouched) {
    const char* src = "#version 330 core";
    CHECK_EQ(SwapShaderVersion(src, "#version 150"), std::string(src));
}

TEST(shader_version_swap_output_has_single_version_directive) {
    const char* src = "\n#version 330 core\nvoid main() {}\n";
    const std::string out = SwapShaderVersion(src, "#version 150");
    std::size_t count = 0;
    for (std::size_t pos = out.find("#version"); pos != std::string::npos;
         pos = out.find("#version", pos + 1)) {
        ++count;
    }
    CHECK_EQ(count, std::size_t(1));
}
