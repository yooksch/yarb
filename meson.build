project(
    'yarb',
    'cpp',
    version: '0.1.0',
    default_options: ['warning_level=3', 'cpp_std=c++23']
)

windows = import('windows')
cc = meson.get_compiler('cpp')

win_rt = declare_dependency(link_args: ['-lwindowsapp', '-lruntimeobject'])
discordgamesdk_dep = declare_dependency(
    dependencies: cc.find_library('discord_game_sdk', dirs: [meson.project_source_root() / 'discordgamesdk' / 'lib']),
    sources: [
        'discordgamesdk\\src\\core.cpp',
        'discordgamesdk\\src\\activity_manager.cpp',
        'discordgamesdk\\src\\types.cpp',
        'discordgamesdk\\src\\achievement_manager.cpp',
        'discordgamesdk\\src\\application_manager.cpp',
        'discordgamesdk\\src\\image_manager.cpp',
        'discordgamesdk\\src\\lobby_manager.cpp',
        'discordgamesdk\\src\\network_manager.cpp',
        'discordgamesdk\\src\\overlay_manager.cpp',
        'discordgamesdk\\src\\relationship_manager.cpp',
        'discordgamesdk\\src\\storage_manager.cpp',
        'discordgamesdk\\src\\store_manager.cpp',
        'discordgamesdk\\src\\user_manager.cpp',
        'discordgamesdk\\src\\voice_manager.cpp',
    ],
    include_directories: include_directories('discordgamesdk\\include')
)

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
        'src\\winhelpers.cpp',
        'src\\discordrpc.cpp',

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
        dependency('openssl', required: true),
        win_rt,
        discordgamesdk_dep
    ],
    install: true,
    install_dir: meson.current_build_dir() / 'release'
)

configure_file(input: 'discordgamesdk\\lib\\discord_game_sdk.dll', copy: true, output: 'discord_game_sdk.dll')
install_data('discordgamesdk\\lib\\discord_game_sdk.dll', install_dir: meson.current_build_dir() / 'release')