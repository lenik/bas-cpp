#cpp_pch = '../' + cpp_pch
exe_inc = include_directories('../src', '..')

app_cpp_list = [
    'asl.cpp',
    'reg.cpp',
    'vols.cpp',
]
foreach app_cpp : app_cpp_list
    app_name = app_cpp.split('/')[-1].replace('.cpp', '')
    if app_name == 'vols' or app_name == 'reg'
        install_dir = get_option('bindir')
        # Same logger as libbas-cpp so vols/reg can tune levels for library log calls.
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

man_list = [
    'vols.1',
]
foreach line : man_list
    install_man(line)
endforeach

bash_list = [
    'vols.bash',
]
foreach line : bash_list
    completion_name = fs.replace_suffix(line.split('/')[-1], '')
    install_data(
        line,
        rename: completion_name,
        install_dir: get_option('datadir') / 'bash-completion' / 'completions',
        install_mode: 'rw-r--r--',
    )
endforeach