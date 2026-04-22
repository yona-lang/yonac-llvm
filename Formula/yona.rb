class Yona < Formula
  desc "Yona programming language compiler targeting LLVM"
  homepage "https://github.com/yona-lang/yonac-llvm"
  url "https://github.com/yona-lang/yonac-llvm/archive/refs/tags/v0.1.0.tar.gz"
  sha256 "PLACEHOLDER" # Update with actual tarball sha256
  license "GPL-3.0-only"
  head "https://github.com/yona-lang/yonac-llvm.git", branch: "master"

  depends_on "cmake" => :build
  depends_on "ninja" => :build
  depends_on "llvm"
  depends_on "pcre2"

  def install
    ENV["CC"] = Formula["llvm"].opt_bin/"clang"
    ENV["CXX"] = Formula["llvm"].opt_bin/"clang++"

    system "cmake", "--preset", "x64-release-macos"
    system "cmake", "--build", "--preset", "build-release-macos"

    bin.install "out/build/x64-release-macos/yonac"
    bin.install "out/build/x64-release-macos/yona"

    # Install standard library
    (lib/"yona/lib").install Dir["lib/Std"]

    # Install runtime source (needed for LTO compilation)
    (lib/"yona/src").install "src/compiled_runtime.c"
    (lib/"yona/src/runtime").install Dir["src/runtime/*.c"]
    (lib/"yona/src/runtime").install Dir["src/runtime/*.h"] if Dir["src/runtime/*.h"].any?
    (lib/"yona/src/runtime/platform").install Dir["src/runtime/platform/*"]
    (lib/"yona/include/yona/runtime").install Dir["include/yona/runtime/*.h"]
  end

  test do
    assert_equal "42\n", shell_output("#{bin}/yonac -e '42'")
    assert_equal "hello\n", shell_output("#{bin}/yonac -e '\"hello\"'")
  end
end
