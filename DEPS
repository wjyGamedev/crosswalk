include_rules = [
  "+content/public",

  "+crypto",
  "+net",
  "+sandbox",
  "+skia",
  "+ui",
  "+v8",
  "+webkit",

  # Allow inclusion of third-party code.
  "+third_party/skia",
  "+third_party/WebKit/public/platform",
  "+third_party/WebKit/public/web",

  # Files generated during Crosswalk build.
  "+grit/xwalk_resources.h",
]

vars = {
}

deps = {
    #'src/xwalk/third_party/tenta/sqlitecrypt': #'git@bitbucket.org:tenta-browser/android-database-sqlitecrypt.git@origin/chromium_77',
    #'src/xwalk/third_party/tenta/meta_fs' : #'git@bitbucket.org:tenta-browser/crosswalk-metafs.git@origin/chromium_77',
    #'src/xwalk/third_party/tenta/chromium_cache' : #'git@bitbucket.org:tenta-browser/crosswalk-metafs-cache.git@origin/chromium_77',
    #'src/xwalk/third_party/tenta/crosswalk_extensions' : #'git@bitbucket.org:tenta-browser/crosswalk-extensions.git@origin/chromium_77',
    #'src/xwalk/third_party/tenta/mimicvpn' : #'git@bitbucket.org:tenta-browser/crosswalk-mimicvpn.git@origin/chromium_77',
}

hooks = [
  {
    # Generate .gclient-xwalk for Crosswalk's dependencies.
    "name": "generate-gclient-xwalk",
    "pattern": ".",
    "action": ["python", "src/xwalk/tools/generate_gclient-xwalk.py"],
  },
  {
    # Fetch Crosswalk dependencies.
    "name": "fetch-deps",
    "pattern": ".",
    "action": ["python", "src/xwalk/tools/fetch_deps.py", "-v"],
  },
#  { see chromium commit 7899e08d3eafd91c8feba31e2a18b715f833675f
    # From src/DEPS: fetch the Google Play services library and, if
    # necessary, prompt the user to accept its EULA.
    # Run here because fetch_deps.py (itself a hook) runs another gclient
    # instance and it all causes buffering problems that do not show the EULA
    # prompt when necessary.
    # Note that this hook is run after all Chromium hooks. This is not a
    # problem as of M48, but this needs to be kept in mind in case things start
    # to break.
#    'action': [
#      'python',
#      'src/build/android/play_services/update.py',
#      'download'
#    ],
#    'pattern':
#      '.',
#    'name':
#      'sdkextras'
#  },
#  {
#    # A change to a .gyp, .gypi, or to GYP itself should run the generator.
#    "name": "gyp-xwalk",
#    "pattern": ".",
#    "action": ["python", "src/xwalk/gyp_xwalk"],
#  }
]
