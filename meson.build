project(
    'bas-cpp',
    'cpp',
    version: '1.0.1',
    default_options: [
        #        'warning_level=0',
        'cpp_std=c++17',
    ],
)

pkgdatadir = get_option('datadir') / meson.project_name()

fs = import('fs')

cpp = meson.get_compiler('cpp')

# `std::filesystem` may require linking `stdc++fs` on older libstdc++ toolchains.
stdcxxfs_deps = []
stdcxxfs_lib = cpp.find_library('stdc++fs', required: false)
if stdcxxfs_lib.found()
    stdcxxfs_deps += stdcxxfs_lib
endif
# Disable all warnings (wx 3.0 and deps are noisy)
cpp_nowarn = []
#cpp_nowarn += cpp.get_supported_arguments(['-w', '/w'])

# libbas-c
subproject('bas-c')
bas_c_dep = dependency('bas-c', required: true)
bas_c_static_dep = dependency('bas-c-static', required: true)

boost_sub = subproject('boost')
boost_json_dep = boost_sub.get_variable('boost_json_dep')
boost_locale_dep = boost_sub.get_variable('boost_locale_dep')

date_dep = subproject('date_tz').get_variable('date_dep')

# ext2fs (e2fsprogs userspace library) for ext2/3/4 image access
ext2fs_dep = dependency('ext2fs', required: true)

# GLib for scriptable UI stack (GList)
glib_dep = dependency('glib-2.0', required: true)

# ICU for charset conversion (CharsetEncoder, CharsetDecoder, etc.)
icu_uc_dep = dependency('icu-uc', required: true)

# libcurl for HTTP/HTTPS/FTP/FTPS (curl license = permissive)
libcurl_dep = dependency('libcurl', required: true)

# OpenSSL (crypto + ssl for HTTPS)
openssl_dep = dependency('openssl', required: true)

# zlib for ZIP support
# zlib is used for decompression, but we'll parse ZIP manually
zlib_dep = dependency('zlib', required: true)

# Precompiled header for wxWidgets
cpp_pch = 'src/bulk.h'

cpp_args = cpp_nowarn + ['-DLOGGER_NAME=cpp', '-DASSETS_NAME=bas_cpp']

cpp_sources = files(
    './src/bas/fmt/FileUtility.cpp',
    './src/bas/fmt/HtmlForm.cpp',
    './src/bas/fmt/JsonForm.cpp',
    './src/bas/fmt/OctetStreamForm.cpp',
    './src/bas/fmt/TextForm.cpp',
    './src/bas/io/BufferedReader.cpp',
    './src/bas/io/ByteBuffer.cpp',
    './src/bas/io/Char32Buffer.cpp',
    './src/bas/io/CharBuffer.cpp',
    './src/bas/io/CharsetDecoder.cpp',
    './src/bas/io/CharsetEncoder.cpp',
    './src/bas/io/ConstBufferInputStream.cpp',
    './src/bas/io/DecodingReadStream.cpp',
    './src/bas/io/EncodingWriteStream.cpp',
    './src/bas/io/EncodingWriteStreamToOutputStream.cpp',
    './src/bas/io/InputSource.cpp',
    './src/bas/io/InputStream.cpp',
    './src/bas/io/InputStreamReader.cpp',
    './src/bas/io/LAInputStream.cpp',
    './src/bas/io/LAInputStreamFromStream.cpp',
    './src/bas/io/LAReader.cpp',
    './src/bas/io/LocalInputSource.cpp',
    './src/bas/io/LocalInputStream.cpp',
    './src/bas/io/LocalOutputStream.cpp',
    './src/bas/io/LocalOutputTarget.cpp',
    './src/bas/io/OutputStream.cpp',
    './src/bas/io/OutputStreamWriter.cpp',
    './src/bas/io/PrintStream.cpp',
    './src/bas/io/PrintWriter.cpp',
    './src/bas/io/ReadStream.cpp',
    './src/bas/io/Reader.cpp',
    './src/bas/io/ReversedReader.cpp',
    './src/bas/io/StringReadStream.cpp',
    './src/bas/io/StringReader.cpp',
    './src/bas/io/StringSource.cpp',
    './src/bas/io/StringTarget.cpp',
    './src/bas/io/StringWriter.cpp',
    './src/bas/io/U32stringReader.cpp',
    './src/bas/io/U32stringSource.cpp',
    './src/bas/io/U32stringTarget.cpp',
    './src/bas/io/U32stringWriter.cpp',
    './src/bas/io/Uint8ArrayInputStream.cpp',
    './src/bas/io/Uint8ArrayOutputStream.cpp',
    './src/bas/io/Uint8ArraySource.cpp',
    './src/bas/io/Uint8ArrayTarget.cpp',
    './src/bas/io/WriteStream.cpp',
    './src/bas/io/Writer.cpp',
    './src/bas/io/WstringReader.cpp',
    './src/bas/io/WstringWriter.cpp',
    './src/bas/net/CurlInputStream.cpp',
    './src/bas/net/CurlOutputStream.cpp',
    './src/bas/net/DataProtocol.cpp',
    './src/bas/net/DataUriInputStream.cpp',
    './src/bas/net/FtpProtocol.cpp',
    './src/bas/net/FtpsProtocol.cpp',
    './src/bas/net/HttpProtocol.cpp',
    './src/bas/net/HttpsProtocol.cpp',
    './src/bas/net/LocalProtocol.cpp',
    './src/bas/net/NetProtocol.cpp',
    './src/bas/net/ProtocolRegistry.cpp',
    './src/bas/net/URI.cpp',
    './src/bas/net/URL.cpp',
    './src/bas/net/URLInputSource.cpp',
    './src/bas/net/URLOutputTarget.cpp',
    './src/bas/proc/ImageFormat.cpp',
    './src/bas/proc/dbgaction.cpp',
    './src/bas/proc/env.cpp',
    './src/bas/script/boost_json.cpp',
    './src/bas/script/json.cpp',
    './src/bas/script/scriptable.cpp',
    './src/bas/util/ClipboardHelper.cpp',
    './src/bas/util/FileTypeDetector.cpp',
    './src/bas/util/LogBuffer.cpp',
    './src/bas/util/Path.cpp',
    './src/bas/util/PathHelper.cpp',
    './src/bas/volume/DirNode.cpp',
    './src/bas/volume/LocalVolume.cpp',
    './src/bas/volume/OverlayVolume.cpp',
    './src/bas/volume/Volume.cpp',
    './src/bas/volume/VolumeFile.cpp',
    './src/bas/volume/VolumeManager.cpp',
    './src/bas/volume/Volume_utils.cpp',
    './src/bas/volume/ext4/Ext4FileInputStream.cpp',
    './src/bas/volume/ext4/Ext4FileOutputStream.cpp',
    './src/bas/volume/ext4/Ext4Volume.cpp',
    './src/bas/volume/ext4/Ext4Volume_write.cpp',
    './src/bas/volume/fat32/fat32_utils.cpp',
    './src/bas/volume/fat32/Fat32FileInputStream.cpp',
    './src/bas/volume/fat32/Fat32FileOutputStream.cpp',
    './src/bas/volume/fat32/Fat32Volume.cpp',
    './src/bas/volume/fat32/Fat32Volume_write.cpp',
    './src/bas/volume/mountinfo.cpp',
    './src/bas/volume/zip/MemoryZip.cpp',
    './src/module.cpp',
)

# Assets will be zipped and linked as a section in the executable
all_sources = cpp_sources

# Build assets.zip every time (no file dependency)
assets_dir = meson.current_source_dir() / 'assets'
build_dir = meson.current_build_dir()
assets_zip = custom_target(
    'assets_zip',
    output: 'assets.zip',
    input: [],
    command: [
        'sh',
        '-c', 'dst=$(readlink -f "@1@"); cd "@0@" && zip -r "$dst" .'.format(assets_dir, '@OUTPUT@'),
    ],
    #build_always : true,
    build_by_default: true,
    install: false,
)

# Compile assembly file that embeds assets.zip using .incbin
# Use configure_file to copy assets.s to build directory
assets_s = configure_file(
    input: 'src/assets.s',
    output: 'assets.s',
    copy: true,
)

# Compile the assembly file (assets.zip will be in the same directory)
as = find_program('as', required: true)
assets_o = custom_target(
    'assets_o',
    output: 'assets.o',
    input: [assets_s, assets_zip],
    command: [as, '-o', '@OUTPUT@', '@INPUT0@'],
    depends: assets_zip,
)

exe_deps = [
    bas_c_dep,
    boost_json_dep,
    boost_locale_dep,
    date_dep,
    ext2fs_dep,
    glib_dep,
    icu_uc_dep,
    libcurl_dep,
    openssl_dep,
    zlib_dep,
] + stdcxxfs_deps

exe_static_deps = [
    bas_c_static_dep,
    boost_json_dep,
    boost_locale_dep,
    date_dep,
    ext2fs_dep,
    glib_dep,
    icu_uc_dep,
    libcurl_dep,
    openssl_dep,
    zlib_dep,
] + stdcxxfs_deps

exe_inc = [
    include_directories('src'),
]

override_options = ['b_lundef=false'] # Allows undefined symbols at link time

lib_bas_cpp = library(
    'bas-cpp',
    cpp_sources,
    assets_o,
    dependencies: exe_deps,
    include_directories: exe_inc,
    cpp_args: cpp_args,
    #cpp_pch: cpp_pch,
    override_options: override_options, # Allows undefined symbols at link time
    version: meson.project_version(),
    soversion: meson.project_version().split('.')[0],
    install: true,
)

lib_bas_cpp_static = static_library(
    'bas-cpp',
    cpp_sources,
    assets_o,
    dependencies: exe_static_deps,
    include_directories: exe_inc,
    cpp_args: cpp_args,
    #cpp_pch: cpp_pch,
    override_options: override_options,
    install: true,
)

bas_cpp_dep = declare_dependency(
    link_with: lib_bas_cpp,
    include_directories: exe_inc,
    dependencies: exe_deps,
)
bas_cpp_static_dep = declare_dependency(
    link_with: lib_bas_cpp_static,
    include_directories: exe_inc,
    dependencies: exe_static_deps,
)

meson.override_dependency('bas-cpp', bas_cpp_dep)
meson.override_dependency('bas-cpp-static', bas_cpp_static_dep)

# ---- pkg-config ----
pkgconfig = import('pkgconfig')
pkgconfig.generate(
    lib_bas_cpp,
    name: 'bas-cpp',
    description: 'C++ base library: I/O, network, volume, formatting, scripting',
    version: meson.project_version(),
    subdirs: 'bas',
    requires: ['bas-c', 'date-tz', 'glib-2.0', 'icu-uc', 'libcurl', 'openssl', 'zlib'],
)

pkgconfig.generate(
    lib_bas_cpp_static,
    name: 'bas-cpp-static',
    description: 'C++ base library: I/O, network, volume, formatting, scripting',
    version: meson.project_version(),
    subdirs: 'bas',
    requires: ['date-tz', 'glib-2.0', 'icu-uc', 'libcurl', 'openssl', 'zlib'],
)

# ---- Headers for libbas-c package (includedir/bas-c) ----
find_other_cmd = run_command(
    'find',
    'src',
    '-name', '*.h',
    '-o',
    '-name', '*.hpp',
    '-o',
    '-type', 'd',
    '-o',
    '-print',
    check: true,
)
other_files = find_other_cmd.stdout().strip().split('\n')

other_list = []
foreach f : other_files
    other_list += f.substring(4)
endforeach

install_subdir(
    'src',
    install_dir: get_option('includedir'),
    exclude_files: other_list,
    strip_directory: true,
)

# Unit tests generator
# Find all *_test.cpp files and build them automatically using the 'includes' utility
# Similar to Makefile pattern rule: %_test: %_test.cpp
#   sources: %_test.cpp $(includes -e -c %_test.cpp)
#   cmdline: $(CXX) $(CXXFLAGS) -o $@ $*_test.cpp `includes -c $*_test.cpp` -licuuc -licui18n

# List of all test source files (manually enumerated since Meson lacks fs module)
# This list is automatically generated from: find src -name '*_test.cpp' | sort
all_test_files = [
    'bas/io/InputStreamReader_test.cpp',
    'bas/io/Uint8ArraySource_test.cpp',
    'bas/io/CharsetEncoder_test.cpp',
    'bas/io/PrintWriter_test.cpp',
    'bas/io/StringReader_test.cpp',
    'bas/io/StringReadStream_test.cpp',
    'bas/io/U32stringWriter_test.cpp',
    'bas/io/LAReader_test.cpp',
    'bas/io/LocalInputStream_test.cpp',
    'bas/io/ReadStream_test.cpp',
    'bas/io/LocalInputSource_test.cpp',
    'bas/io/StringSource_test.cpp',
    'bas/io/BufferedReader_test.cpp',
    'bas/io/ReversedReader_test2.cpp',
    'bas/io/StringTarget_test.cpp',
    'bas/io/Char32Buffer_test.cpp',
    'bas/io/EncodingWriteStream_test.cpp',
    'bas/io/Reader_test.cpp',
    'bas/io/Uint8ArrayTarget_test.cpp',
    'bas/io/U32stringSource_test.cpp',
    'bas/io/OutputStreamWriter_test.cpp',
    'bas/io/WriteStream_test.cpp',
    'bas/io/Uint8ArrayOutputStream_test.cpp',
    'bas/io/StringWriter_test.cpp',
    'bas/io/WstringWriter_test.cpp',
    'bas/io/LAInputStream_test.cpp',
    'bas/io/LocalOutputStream_test.cpp',
    'bas/io/ByteBuffer_test.cpp',
    'bas/io/InputStream_test.cpp',
    'bas/io/Uint8ArrayInputStream_test.cpp',
    'bas/io/U32stringReader_test.cpp',
    'bas/io/LAInputStreamFromStream_test.cpp',
    'bas/io/Writer_test.cpp',
    'bas/io/LocalOutputTarget_test.cpp',
    'bas/io/CharsetDecoder_test.cpp',
    'bas/io/WstringReader_test.cpp',
    'bas/io/ReversedReader_test.cpp',
    'bas/io/ConstBufferInputStream_test.cpp',
    'bas/io/U32stringTarget_test.cpp',
    'bas/io/PrintStream_test.cpp',
    'bas/io/DecodingReadStream_test.cpp',
    'bas/io/InputSource_test.cpp',
    'bas/io/CharBuffer_test.cpp',
    'bas/util/Path_test.cpp',
    'bas/util/PathHelper_test.cpp',
    'bas/util/LogBuffer_test.cpp',
    'bas/util/FileTypeDetector_test.cpp',
    'bas/net/URI_test.cpp',
    'bas/net/ProtocolRegistry_test.cpp',
    'bas/net/LocalProtocol_test.cpp',
    'bas/net/URL_test.cpp',
    'bas/net/DataUriInputStream_test.cpp',
    'bas/net/DataProtocol_test.cpp',
    'bas/script/json_test.cpp',
    'bas/volume/ext4/Ext4_write_test.cpp',
    'bas/volume/ext4/Ext4_test.cpp',
    'bas/volume/ext4/Ext4_forest_test.cpp',
    'bas/volume/fat32/Fat32_persistence_test.cpp',
    'bas/volume/fat32/Fat32_write_test.cpp',
    'bas/volume/fat32/Fat32_test.cpp',
    'bas/volume/fat32/Fat32_forest_test.cpp',
]

# Build each test using the 'includes' utility to get source dependencies
foreach test_rel_path : all_test_files
    test_cpp = 'src/' + test_rel_path

    # Extract test name (e.g., ByteBuffer_test from src/io/ByteBuffer_test.cpp)
    path_parts = test_rel_path.split('/')
    test_file = path_parts[path_parts.length() - 1]
    test_name = test_file.replace('.cpp', '')

    # Use 'includes' utility to get implementation dependencies
    # 'includes -e -c test.cpp' returns source files, one per line (not -I include paths)
    includes_result = run_command('includes', '-e', '-c', test_cpp, check: false)

    impl_deps = []
    if includes_result.returncode() == 0
        # Parse the output: each line is a .c/.cpp file path
        output = includes_result.stdout()
        lines = output.split('\n')
        foreach line : lines
            line = line.strip()
            # Keep non-empty lines that are .c/.cpp files
            if line.endswith('.cpp')
                impl_deps += line
            endif
        endforeach
    endif

    # Combine test file with implementation dependencies from 'includes'
    all_sources = [test_cpp] + impl_deps

    # Create the executable (LOGGER_NAME so program.h and logger.c work in tests)
    test_exe = executable(
        test_name,
        all_sources,
        include_directories: exe_inc,
        link_with: lib_bas_cpp,
        dependencies: exe_deps,
        cpp_args: cpp_nowarn
        + ['-DLOGGER_NAME=test', '-DASSETS_NAME=bas_cpp'],
        #cpp_pch: cpp_pch,
        install: false,
    )

    # Register the test with meson
    test(test_name, test_exe)
endforeach

# --- Applications in app/ (one .cpp = one program), install to bindir + manpages to mandir ---
find_app = run_command('find', 'app', '-maxdepth', '1', '-name', '*.cpp', check: false)
app_cpp_list = []
if find_app.returncode() == 0
    foreach line : find_app.stdout().strip().split('\n')
        if line != ''
            app_cpp_list += line
        endif
    endforeach
endif

foreach app_cpp : app_cpp_list
    app_name = app_cpp.split('/')[-1].replace('.cpp', '')
    if app_name == 'vols'
        install_dir = get_option('bindir')
        # Same logger as libbas-cpp so vols can tune levels for VolumeManager::logwarn etc.
        app_logger = 'cpp'
    else
        install_dir = pkgdatadir / 'app'
        app_logger = app_name
    endif
    executable(
        app_name,
        app_cpp,
        include_directories: exe_inc,
        link_with: lib_bas_cpp,
        dependencies: exe_deps,
        cpp_args: cpp_nowarn
        + ['-DLOGGER_NAME=' + app_logger, '-DASSETS_NAME=bas_cpp'],
        #cpp_pch: cpp_pch,
        install: true,
        install_dir: install_dir,
    )
endforeach

# Manpages: app/*.1 -> install to mandir
find_man = run_command('find', 'app', '-maxdepth', '1', '-name', '*.1', check: false)
if find_man.returncode() == 0
    foreach line : find_man.stdout().strip().split('\n')
        if line == ''
            continue
        endif
        install_man(line)
    endforeach
endif

# ---- Bash completion (install to datadir/bash-completion/completions) ----
find_bash = run_command('find', 'app', '-maxdepth', '1', '-name', '*.bash', check: false)
app_bash_list = []
if find_bash.returncode() == 0
    foreach line : find_bash.stdout().strip().split('\n')
        if line == ''
            continue
        endif
        completion_name = fs.replace_suffix(line.split('/')[-1], '')
        install_data(
            line,
            rename: completion_name,
            install_dir: get_option('datadir') / 'bash-completion' / 'completions',
            install_mode: 'rw-r--r--',
        )
    endforeach
endif