Name:           yona
Version:        0.1.0
Release:        1%{?dist}
Summary:        Yona programming language compiler targeting LLVM
License:        GPL-3.0-only
URL:            https://github.com/yona-lang/yonac-llvm
Source0:        %{url}/archive/v%{version}/%{name}-%{version}.tar.gz

BuildRequires:  cmake >= 3.10
BuildRequires:  ninja-build
BuildRequires:  clang
BuildRequires:  lld
BuildRequires:  llvm-devel >= 16
BuildRequires:  llvm-static
BuildRequires:  pcre2-devel

Requires:       llvm-libs >= 16
Requires:       clang
Requires:       lld
Requires:       pcre2

%description
Yona is a compiled functional programming language that targets LLVM.
Features include algebraic data types, pattern matching, traits,
persistent data structures, async I/O via io_uring, and a comprehensive
standard library.

%prep
%autosetup -n yonac-llvm-%{version}

%build
cmake --preset x64-release-linux
cmake --build --preset build-release-linux

%check
cd out/build/x64-release-linux
ctest --output-on-failure

%install
install -Dm755 out/build/x64-release-linux/yonac %{buildroot}%{_bindir}/yonac
install -Dm755 out/build/x64-release-linux/yona %{buildroot}%{_bindir}/yona

# Standard library
install -d %{buildroot}%{_libdir}/yona/lib/Std
install -Dm644 lib/Std/*.yona %{buildroot}%{_libdir}/yona/lib/Std/ 2>/dev/null || true
install -Dm644 lib/Std/*.yonai %{buildroot}%{_libdir}/yona/lib/Std/

# Runtime source (for LTO)
install -d %{buildroot}%{_libdir}/yona/src/runtime/platform
install -d %{buildroot}%{_libdir}/yona/include/yona/runtime
install -d %{buildroot}%{_libdir}/yona/runtime
cp out/build/x64-release-linux/runtime/* %{buildroot}%{_libdir}/yona/runtime/ 2>/dev/null || true
install -Dm644 src/compiled_runtime.c %{buildroot}%{_libdir}/yona/src/
install -Dm644 src/runtime/*.c %{buildroot}%{_libdir}/yona/src/runtime/ 2>/dev/null || true
install -Dm644 src/runtime/platform/*.c %{buildroot}%{_libdir}/yona/src/runtime/platform/
install -Dm644 include/yona/runtime/*.h %{buildroot}%{_libdir}/yona/include/yona/runtime/

%files
%license LICENSE.txt
%doc README.md docs/
%{_bindir}/yonac
%{_bindir}/yona
%{_libdir}/yona/

%changelog
* Sun Apr 06 2025 Yona Team <team@yona-lang.org> - 0.1.0-1
- Initial package
- Compiler (yonac) and REPL (yona)
- 27 stdlib modules, 290+ exported functions
- Persistent data structures (RBT Seq, HAMT Dict/Set)
- Async I/O via io_uring, thread pool
- PCRE2-backed regex module
- Non-blocking Process module
