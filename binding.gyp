{
  "variables": {
    "node_os": "<!(node -p \"'<(OS)' === 'win' ? 'win32' : '<(OS)' === 'mac' ? 'darwin' : 'linux'\")",
    "is_electron": "<!(node -p \"'<(nodedir)'.includes('electron')\")"
  },
  "targets": [
    {
      "target_name": "uws_<(node_os)_<(target_arch)_<(target_node_module_version)",
      "defines": [
        "UWS_WITH_PROXY",
        "LIBUS_USE_LIBUV"
      ],
      "include_dirs": [
        "uWebSockets/uSockets/src",
        "uWebSockets/src"
      ],
      "sources": [
        "<!@(node -p \"require('fs').readdirSync('./uWebSockets/uSockets/src').filter(f=>/.c$/.exec(f)).map(f=>'uWebSockets/uSockets/src/'+f).join(' ')\")",
        "<!@(node -p \"require('fs').readdirSync('./uWebSockets/uSockets/src/eventing').filter(f=>/.c$/.exec(f)).map(f=>'uWebSockets/uSockets/src/eventing/'+f).join(' ')\")",
        "<!@(node -p \"require('fs').readdirSync('./uWebSockets/uSockets/src/crypto').filter(f=>/.c$/.exec(f)).map(f=>'uWebSockets/uSockets/src/crypto/'+f).join(' ')\")",
        "src/addon.cpp",
        "uWebSockets/uSockets/src/crypto/sni_tree.cpp"
      ],
      "conditions": [
        [
          "OS=='win'",
          {
            "cflags": [
              "/W3",
              "/EHsc",
              "/Ox"
            ],
            "msbuild_settings": {
              "ClCompile": {
                "LanguageStandard": "stdcpp17"
              }
            }
          }
        ],
        [
          "OS=='linux'",
          {
            "cflags": [
              "-flto",
              "-O3",
              "-c",
              "-fPIC"
            ],
            "cflags_cc": [
              "--std=c++17"
            ],
          }
        ],
        [
          "OS=='mac'",
          {
            "xcode_settings": {
              'GCC_ENABLE_CPP_EXCEPTIONS': 'YES',
              "CLANG_CXX_LIBRARY": "libc++",
              "CLANG_CXX_LANGUAGE_STANDARD":"c++17",
              'MACOSX_DEPLOYMENT_TARGET': '10.14'
            },
            "cflags": [
              "-flto",
              "-O3",
              "-c",
              "-fPIC"
            ],
            "cflags_cc": [
              "--std=c++17"
            ],
          }
        ],
        [
          "is_electron=='true'",
          {
            "defines": [
              "LIBUS_NO_SSL",
              "UWS_NO_ZLIB"
            ]
          }
        ],
        [
          "is_electron=='false'",
          {
            "defines": [
              "LIBUS_NO_SSL",
              "UWS_NO_ZLIB"
            ]
          }
        ],
      ]
    }
  ],
  "conditions": [
    [
      "OS=='win'",
      {
        "variables": {
          "target_node_module_version": "<!(node -p \"require('fs').readFileSync(require('path').join(String.raw`<(nodedir)`, 'include', 'node', 'node_version.h'), { encoding: 'utf8' }).match(/#define NODE_MODULE_VERSION ([0-9]+)/)[1]\")",
        }
      }
    ],
    [
      "OS!='win'",
      {
        "variables": {
          "target_node_module_version": "<!(node -p \"require('fs').readFileSync(require('path').join(String.raw\`<(nodedir)\`, 'include', 'node', 'node_version.h'), { encoding: 'utf8' }).match(/#define NODE_MODULE_VERSION ([0-9]+)/)[1]\")",
        }
      }
    ]
  ]
}