#cpp_pch = '../' + cpp_pch
exe_inc = include_directories('../src', '..')

# Unit tests generator
# Find all *_test.cpp files and build them automatically using the 'includes' utility
# Similar to Makefile pattern rule: %_test: %_test.cpp
#   sources: %_test.cpp $(includes -e -c %_test.cpp)
#   cmdline: $(CXX) $(CXXFLAGS) -o $@ $*_test.cpp `includes -c $*_test.cpp` -licuuc -licui18n

# List of all test source files (manually enumerated since Meson lacks fs module)
# This list is automatically generated from: find src -name '*_test.cpp' | sort
test_cpps = [
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
    'bas/volume/exfat/Exfat_test.cpp',
    'bas/volume/exfat/Exfat_simple_test.cpp',
    'bas/volume/fat32/Fat32_persistence_test.cpp',
    'bas/volume/fat32/Fat32_write_test.cpp',
    'bas/volume/fat32/Fat32_test.cpp',
    'bas/volume/fat32/Fat32_forest_test.cpp',
]

# Build each test using the 'includes' utility to get source dependencies
foreach test_cpp : test_cpps
    # Extract test name (e.g., ByteBuffer_test from src/io/ByteBuffer_test.cpp)
    split = test_cpp.split('/')
    base = split[split.length() - 1]
    dir = fs.parent(test_cpp)
    stem = base.replace('.cpp', '')
    sibling_dir = '../src' / dir

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
        stem,
        all_sources,
        include_directories: [sibling_dir, exe_inc],
        link_with: lib_bas_cpp,
        dependencies: exe_deps,
        cpp_args: cpp_nowarn + ['-DLOGGER_NAME=test', '-DASSETS_NAME=bas_cpp'],
        #cpp_pch: cpp_pch,
        install: false,
    )

    # Register the test with meson
    test(stem, test_exe)
endforeach