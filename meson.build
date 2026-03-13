project('bas-cpp', 'cpp', 
    version : '1.0.0',
    default_options : [
#        'warning_level=0', 
        'cpp_std=c++17'])

pkgdatadir = get_option('datadir') / meson.project_name()

fs = import('fs')

cpp = meson.get_compiler('cpp')
# Disable all warnings (wx 3.0 and deps are noisy)
cpp_nowarn = []
#cpp_nowarn += cpp.get_supported_arguments(['-w', '/w'])

# libbas-c
    bas_dep = dependency('bas-c', required : true)

# GLib for scriptable UI stack (GList)
    glib_dep = dependency('glib-2.0', required : true)

# ICU for charset conversion (CharsetEncoder, CharsetDecoder, etc.)
    icu_uc_dep = dependency('icu-uc', required : true)

# libcurl for HTTP/HTTPS/FTP/FTPS (curl license = permissive)
    libcurl_dep = dependency('libcurl', required : true)

# OpenSSL (crypto + ssl for HTTPS)
    openssl_dep = dependency('openssl', required : true)

# boost libraries
    # use vendored boost libraries if boost-system <= 1.67
    boost_system_dep = dependency('boost', modules: ['system'], required: true)

    wx_dep = dependency('wxwidgets', required : true)

    if not boost_system_dep.found() or (boost_system_dep.version() < '1.68' or wx_dep.version() < '3.1')
        boost_inc = [
            include_directories('third_party/boost_json/include'),
            include_directories('third_party/boost_core/include'),
            include_directories('third_party/boost_assert/include'),
        ]

        # Boost.System (error_code) used by vendored Boost.JSON and JsonForm
        boost_dep = dependency('boost', modules : [
            'locale',
            'system',
            ], required : true)

    else
        boost_dep = dependency('boost', modules : [
            'assert',
            'core',
            'json',
            'locale',
            'system',
            ], required : true)
    endif

# zlib for ZIP support
    # zlib is used for decompression, but we'll parse ZIP manually
    zlib_dep = dependency('zlib', required : true)

cpp_sources = files(
    './src/module.cpp',
    './src/fmt/JsonForm.cpp',
    './src/fmt/TextForm.cpp',
    './src/fmt/FileUtility.cpp',
    './src/fmt/HtmlForm.cpp',
    './src/fmt/OctetStreamForm.cpp',
    './src/proc/ImageFormat.cpp',
    './src/proc/Assets.cpp',
    './src/proc/dbgaction.cpp',
    './src/io/LAInputStream.cpp',
    './src/io/CharsetDecoder.cpp',
    './src/io/EncodingWriteStreamToOutputStream.cpp',
    './src/io/U32stringTarget.cpp',
    './src/io/PrintWriter.cpp',
    './src/io/OutputStream.cpp',
    './src/io/ByteBuffer.cpp',
    './src/io/Writer.cpp',
    './src/io/StringWriter.cpp',
    './src/io/CharBuffer.cpp',
    './src/io/WstringReader.cpp',
    './src/io/U32stringReader.cpp',
    './src/io/U32stringSource.cpp',
    './src/io/LocalOutputTarget.cpp',
    './src/io/WstringWriter.cpp',
    './src/io/InputStream.cpp',
    './src/io/LocalInputSource.cpp',
    './src/io/StringReadStream.cpp',
    './src/io/LAReader.cpp',
    './src/io/U32stringWriter.cpp',
    './src/io/LAInputStreamFromStream.cpp',
    './src/io/ConstBufferInputStream.cpp',
    './src/io/ReversedReader.cpp',
    './src/io/Char32Buffer.cpp',
    './src/io/StringSource.cpp',
    './src/io/LocalOutputStream.cpp',
    './src/io/BufferedReader.cpp',
    './src/io/Reader.cpp',
    './src/io/EncodingWriteStream.cpp',
    './src/io/Uint8ArrayInputStream.cpp',
    './src/io/ReadStream.cpp',
    './src/io/Uint8ArraySource.cpp',
    './src/io/StringTarget.cpp',
    './src/io/Uint8ArrayTarget.cpp',
    './src/io/CharsetEncoder.cpp',
    './src/io/PrintStream.cpp',
    './src/io/LocalInputStream.cpp',
    './src/io/InputStreamReader.cpp',
    './src/io/StringReader.cpp',
    './src/io/InputSource.cpp',
    './src/io/Uint8ArrayOutputStream.cpp',
    './src/io/OutputStreamWriter.cpp',
    './src/io/WriteStream.cpp',
    './src/io/DecodingReadStream.cpp',
    './src/util/FileTypeDetector.cpp',
    './src/util/LogBuffer.cpp',
    './src/util/ClipboardHelper.cpp',
    './src/util/PathHelper.cpp',
    './src/util/Path.cpp',
    './src/net/DataUriInputStream.cpp',
    './src/net/URLOutputTarget.cpp',
    './src/net/LocalProtocol.cpp',
    './src/net/URI.cpp',
    './src/net/CurlOutputStream.cpp',
    './src/net/FtpProtocol.cpp',
    './src/net/DataProtocol.cpp',
    './src/net/URLInputSource.cpp',
    './src/net/ProtocolRegistry.cpp',
    './src/net/NetProtocol.cpp',
    './src/net/HttpsProtocol.cpp',
    './src/net/URL.cpp',
    './src/net/HttpProtocol.cpp',
    './src/net/CurlInputStream.cpp',
    './src/net/FtpsProtocol.cpp',
    './src/script/json.cpp',
    './src/script/scriptable.cpp',
    './src/script/boost_json.cpp',
    './src/volume/LocalVolume.cpp',
    './src/volume/MemoryZip.cpp',
    './src/volume/Volume.cpp',
    './src/volume/VolumeFile.cpp',
    './src/volume/VolumeManager.cpp',
)

# Assets will be zipped and linked as a section in the executable
all_sources = cpp_sources

# Build assets.zip every time (no file dependency)
assets_dir = meson.current_source_dir() / 'assets'
build_dir = meson.current_build_dir()
assets_zip = custom_target(
    'assets_zip',
    output : 'assets.zip',
    input : [],
    command : [
        'sh', '-c',
        'dst=$(readlink -f "@1@"); cd "@0@" && zip -r "$dst" .'.format(assets_dir, '@OUTPUT@')
    ],
    #build_always : true,
    build_by_default : true,
    install : false,
)

# Compile assembly file that embeds assets.zip using .incbin
# Use configure_file to copy assets.s to build directory
assets_s = configure_file(
    input : 'app/assets.s',
    output : 'assets.s',
    copy : true,
)

# Compile the assembly file (assets.zip will be in the same directory)
as = find_program('as', required : true)
assets_o = custom_target(
    'assets_o',
    output : 'assets.o',
    input : [assets_s, assets_zip],
    command : [
        as,
        '-o', '@OUTPUT@',
        '@INPUT0@'
    ],
    depends : assets_zip,
)

exe_deps = [
    bas_dep,
    boost_dep,
    glib_dep,
    icu_uc_dep,
    libcurl_dep,
    openssl_dep,
    zlib_dep,
]

exe_inc = [
    include_directories('src'),
] + boost_inc

lib_bas_cpp = library('bas-cpp',
    cpp_sources,
    dependencies : exe_deps,
    include_directories : exe_inc,
    cpp_args : cpp_nowarn + ['-DLOGGER_NAME=cpp'],
    override_options: ['b_lundef=false'], # Allows undefined symbols at link time
    version : meson.project_version(),
    soversion : meson.project_version().split('.')[0],
    install : true,
)

bas_cpp_dep = declare_dependency(
    link_with: lib_bas_cpp,
    include_directories: exe_inc
)

# ---- pkg-config ----
pkgconfig = import('pkgconfig')
pkgconfig.generate(
    lib_bas_cpp,
    name : 'bas-cpp',
    description : 'C++ base library: I/O, network, volume, formatting, scripting',
    version : meson.project_version(),
    subdirs : 'bas',
    requires : ['bas-c', 'glib-2.0', 'libcurl', 'openssl', 'zlib', 'icu-uc'],
)

# ---- Headers for libbas-c package (includedir/bas-c) ----
find_other_cmd = run_command('find', 'src',
          '-name', '*.h',
    '-o', '-name', '*.hpp',
    '-o', '-type', 'd',
    '-o', '-print',
    check: true)
other_files = find_other_cmd.stdout().strip().split('\n')

other_list = []
foreach f : other_files
    other_list += f.substring(4)
endforeach

install_subdir('src',
    install_dir : get_option('includedir') / 'bas',
    exclude_files : other_list,
    strip_directory : true,
)

# Unit tests generator
# Find all *_test.cpp files and build them automatically using the 'includes' utility
# Similar to Makefile pattern rule: %_test: %_test.cpp 
#   sources: %_test.cpp $(includes -e -c %_test.cpp)
#   cmdline: $(CXX) $(CXXFLAGS) -o $@ $*_test.cpp `includes -c $*_test.cpp` -licuuc -licui18n

# List of all test source files (manually enumerated since Meson lacks fs module)
# This list is automatically generated from: find src -name '*_test.cpp' | sort
all_test_files = [
    'io/InputStreamReader_test.cpp',
    'io/Uint8ArraySource_test.cpp',
    'io/CharsetEncoder_test.cpp',
    'io/PrintWriter_test.cpp',
    'io/StringReader_test.cpp',
    'io/StringReadStream_test.cpp',
    'io/U32stringWriter_test.cpp',
    'io/LAReader_test.cpp',
    'io/LocalInputStream_test.cpp',
    'io/ReadStream_test.cpp',
    'io/LocalInputSource_test.cpp',
    'io/StringSource_test.cpp',
    'io/BufferedReader_test.cpp',
    'io/ReversedReader_test2.cpp',
    'io/StringTarget_test.cpp',
    'io/Char32Buffer_test.cpp',
    'io/EncodingWriteStream_test.cpp',
    'io/Reader_test.cpp',
    'io/Uint8ArrayTarget_test.cpp',
    'io/U32stringSource_test.cpp',
    'io/OutputStreamWriter_test.cpp',
    'io/WriteStream_test.cpp',
    'io/Uint8ArrayOutputStream_test.cpp',
    'io/StringWriter_test.cpp',
    'io/WstringWriter_test.cpp',
    'io/LAInputStream_test.cpp',
    'io/LocalOutputStream_test.cpp',
    'io/ByteBuffer_test.cpp',
    'io/InputStream_test.cpp',
    'io/Uint8ArrayInputStream_test.cpp',
    'io/U32stringReader_test.cpp',
    'io/LAInputStreamFromStream_test.cpp',
    'io/Writer_test.cpp',
    'io/LocalOutputTarget_test.cpp',
    'io/CharsetDecoder_test.cpp',
    'io/WstringReader_test.cpp',
    'io/ReversedReader_test.cpp',
    'io/ConstBufferInputStream_test.cpp',
    'io/U32stringTarget_test.cpp',
    'io/PrintStream_test.cpp',
    'io/DecodingReadStream_test.cpp',
    'io/InputSource_test.cpp',
    'io/CharBuffer_test.cpp',
    'util/Path_test.cpp',
    'util/PathHelper_test.cpp',
    'util/LogBuffer_test.cpp',
    'util/FileTypeDetector_test.cpp',
    'net/URI_test.cpp',
    'net/ProtocolRegistry_test.cpp',
    'net/LocalProtocol_test.cpp',
    'net/URL_test.cpp',
    'net/DataUriInputStream_test.cpp',
    'net/DataProtocol_test.cpp',
    'script/json_test.cpp',
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
    includes_result = run_command('includes', '-e', '-c', test_cpp, check : false)
    
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
    test_exe = executable(test_name,
        all_sources,
        include_directories : exe_inc,
        dependencies : exe_deps,
        cpp_args : cpp_nowarn + ['-DLOGGER_NAME=test'],
        install : false,
    )
    
    # Register the test with meson
    test(test_name, test_exe)
endforeach

# --- Applications in app/ (one .cpp = one program), install to bindir + manpages to mandir ---
find_app = run_command('find', 'app', '-maxdepth', '1', '-name', '*.cpp', check : false)
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
    else
        install_dir = pkgdatadir / 'app'
    endif
    app_exe = executable(app_name,
        app_cpp,
        assets_o,
        link_with : lib_bas_cpp,
        include_directories : exe_inc,
        dependencies : exe_deps,
        cpp_args : cpp_nowarn + ['-DLOGGER_NAME=' + app_name],
        install : true,
        install_dir : install_dir,
    )
endforeach

# Manpages: app/*.1 -> install to mandir
find_man = run_command('find', 'app', '-maxdepth', '1', '-name', '*.1', check : false)
if find_man.returncode() == 0
    foreach line : find_man.stdout().strip().split('\n')
        if line == ''
            continue
        endif
        install_man(line)
    endforeach
endif

# ---- Bash completion (install to datadir/bash-completion/completions) ----
find_bash = run_command('find', 'app', '-maxdepth', '1', '-name', '*.bash', check : false)
app_bash_list = []
if find_bash.returncode() == 0
    foreach line : find_bash.stdout().strip().split('\n')
        if line == ''
            continue
        endif
        completion_name = fs.replace_suffix(line.split('/')[-1], '')
        install_data(
            line,
            rename : completion_name,
            install_dir : get_option('datadir') / 'bash-completion' / 'completions',
            install_mode : 'rw-r--r--',
        )
    endforeach
endif

