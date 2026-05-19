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

if fs.is_dir('subprojects/bas-c')
    subproject('bas-c')
endif

# libbas-c
bas_c_dep = dependency('bas-c', required: true)
bas_c_static_dep = dependency('bas-c-static', required: true)

if fs.is_dir('subprojects/boost')
    boost_sub = subproject('boost')
    boost_json_dep = boost_sub.get_variable('boost_json_dep')
    boost_locale_dep = boost_sub.get_variable('boost_locale_dep')
    boost_dep = declare_dependency(dependencies: [boost_json_dep, boost_locale_dep])
else
    boost_dep = dependency('boost', modules: ['json', 'locale'], required: true)
endif

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

unconst_dep = dependency('unconst', required: true)

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
    './src/bas/reg/JsonRegistry.cpp',
    './src/bas/reg/LocalRegistry.cpp',
    './src/bas/reg/Sub.cpp',
    './src/bas/reg/VolumeRegistry.cpp',
    './src/bas/reg/variant.cpp',
    './src/bas/reg/strings.cpp',
    './src/bas/reg/Registry.cpp',
    './src/bas/reg/path.cpp',
    './src/bas/script/boost_json.cpp',
    './src/bas/script/json.cpp',
    './src/bas/script/scriptable.cpp',
    './src/bas/time/Duration.cpp',
    './src/bas/time/Instant.cpp',
    './src/bas/time/LocalDate.cpp',
    './src/bas/time/LocalDateTime.cpp',
    './src/bas/time/LocalTime.cpp',
    './src/bas/time/OffsetDateTime.cpp',
    './src/bas/time/OffsetTime.cpp',
    './src/bas/time/Period.cpp',
    './src/bas/time/Temporal.cpp',
    './src/bas/time/TemporalAccessor.cpp',
    './src/bas/time/TimeApiDefaults.cpp',
    './src/bas/time/TimeTypes.cpp',
    './src/bas/time/ZoneId.cpp',
    './src/bas/time/ZonedDateTime.cpp',
    './src/bas/time/ZonedTime.cpp',
    './src/bas/util/ClipboardHelper.cpp',
    './src/bas/util/FileTypeDetector.cpp',
    './src/bas/util/LogBuffer.cpp',
    './src/bas/util/Path.cpp',
    './src/bas/util/PathHelper.cpp',
    './src/bas/util/encoding.cpp',
    './src/bas/util/urls.cpp',
    './src/bas/util/uuid.cpp',
    './src/bas/volume/BlockDevice.cpp',
    './src/bas/volume/DirNode.cpp',
    './src/bas/volume/LocalVolume.cpp',
    './src/bas/volume/OverlayVolume.cpp',
    './src/bas/volume/Volume.cpp',
    './src/bas/volume/VolumeFile.cpp',
    './src/bas/volume/VolumeManager.cpp',
    './src/bas/volume/Volume_utils.cpp',
    './src/bas/volume/dev/FileDevice.cpp',
    './src/bas/volume/dev/LoopDevice.cpp',
    './src/bas/volume/dev/MMapDevice.cpp',
    './src/bas/volume/dev/MemDevice.cpp',
    './src/bas/volume/exfat/ExfatClusterManager.cpp',
    './src/bas/volume/exfat/ExfatFileInputStream.cpp',
    './src/bas/volume/exfat/ExfatFileOutputStream.cpp',
    './src/bas/volume/exfat/ExfatVolume.cpp',
    './src/bas/volume/exfat/ExfatVolume_stubs.cpp',
    './src/bas/volume/exfat/ExfatVolume_write.cpp',
    './src/bas/volume/ext4/Ext4FileInputStream.cpp',
    './src/bas/volume/ext4/Ext4FileOutputStream.cpp',
    './src/bas/volume/ext4/Ext4IoManager.cpp',
    './src/bas/volume/ext4/Ext4Volume.cpp',
    './src/bas/volume/ext4/Ext4Volume_write.cpp',
    './src/bas/volume/fat32/Fat32ClusterManager.cpp',
    './src/bas/volume/fat32/Fat32FileInputStream.cpp',
    './src/bas/volume/fat32/Fat32FileOutputStream.cpp',
    './src/bas/volume/fat32/Fat32Volume.cpp',
    './src/bas/volume/fat32/Fat32Volume_write.cpp',
    './src/bas/volume/fat32/fat32_utils.cpp',
    './src/bas/volume/mountinfo.cpp',
    './src/bas/volume/overlay_ls.cpp',
    './src/bas/volume/zip/ZipVolume.cpp',
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
    command: [
        'sh',
        '-c', 'cd "$1" && "$2" -o assets.o assets.s',
        'sh',
        build_dir,
        as,
    ],
    depends: assets_zip,
)

exe_deps = [
    bas_c_dep,
    boost_dep,
    date_dep,
    ext2fs_dep,
    glib_dep,
    icu_uc_dep,
    libcurl_dep,
    openssl_dep,
    unconst_dep,
    zlib_dep,
] + stdcxxfs_deps

exe_static_deps = [
    bas_c_static_dep,
    boost_dep,
    date_dep,
    ext2fs_dep,
    glib_dep,
    icu_uc_dep,
    libcurl_dep,
    openssl_dep,
    unconst_dep,
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
    requires: ['bas-c', 'glib-2.0', 'icu-uc', 'libcurl', 'openssl', 'zlib'],
)

pkgconfig.generate(
    lib_bas_cpp_static,
    name: 'bas-cpp-static',
    description: 'C++ base library: I/O, network, volume, formatting, scripting',
    version: meson.project_version(),
    subdirs: 'bas',
    requires: ['glib-2.0', 'icu-uc', 'libcurl', 'openssl', 'zlib'],
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

subdir('app')
subdir('tests')
