project('CAN_BMS_Intertface_TDD', ['c'],
  version : '0.1',
  default_options : ['warning_level=3', 'c_std=c11', 'cpp_std=c++11'])

# 1. Pfade definieren
unity_inc = include_directories('unity')
# Geändert: Jetzt auf den Inc-Unterordner im Mock-Verzeichnis
mock_inc  = include_directories('mocks/Inc') 
app_inc   = include_directories('../include')

# 2. Unity Framework (bleibt gleich)
unity_src = files('unity/unity.c', 'unity/unity_fixture.c')
unity_lib = static_library('unity', unity_src, include_directories : unity_inc)

# 3. Quellen sammeln
test_sources = files(
  'main_test.c',
  'modules/bms_communication/bms_communication_test.c',
  'modules/bms_communication/bms_communication_test_runner.c',
  '../src/bms_communication.c',
  # MOCK IMPLEMENTATIONS
  'mocks/Src/can_mock.c',

)

# 4. Das Test-Executable
test_exe = executable('run_firmware_tests',
  test_sources,
  include_directories : [
    unity_inc, 
    mock_inc,   # Priorität 1: Mocks
    app_inc     # Priorität 2: Echte Header (falls kein Mock existiert)
  ], 
  link_with : unity_lib
)

# 5. Registrierung
test('firmware_logic_tests', test_exe, args : ['-v'])