project(
    'yarb',
    'cpp',
    version: '0.1.0',
    default_options: ['warning_level=3', 'cpp_std=c++23']
)

windows = import('windows')

executable('yarb',
    [
        'src\\main.cpp',
        'src\\http.cpp',
        'src\\game.cpp',
        'src\\config.cpp',
        'src\\crypto.cpp',
        'src\\log.cpp',
        'src\\paths.cpp',
        'src\\gui.cpp',
        'src\\modmanager.cpp',

        # Dear ImGUI

        'imgui\\imgui.cpp',
        'imgui\\imgui_draw.cpp',
        'imgui\\imgui_tables.cpp',
        'imgui\\imgui_widgets.cpp',
        'imgui\\backends\\imgui_impl_opengl3.cpp',
        'imgui\\backends\\imgui_impl_win32.cpp',
    ],
    windows.compile_resources('res\\yarb.rc'),
    include_directories: include_directories('include', 'imgui'),
    dependencies: [
        dependency('nlohmann_json', required: true, fallback: ['nlohmann-json']),
        dependency('fmt', required: true),
        dependency('libzip', required: true),
        dependency('gl', required: true),
        dependency('libcurl', required: true),
        dependency('openssl', required: true)
    ]
)
