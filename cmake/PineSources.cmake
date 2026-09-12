set(PINE_CORE_SOURCES
  src/core/ApplicationManager.cpp src/core/Configuration.cpp src/core/Logger.cpp
  src/input/TextInputSession.cpp src/input/TextInputManager.cpp
  src/services/BatteryService.cpp src/services/BluetoothService.cpp src/services/CameraService.cpp
  src/services/AudioService.cpp src/services/NetworkService.cpp src/services/FileService.cpp
  src/services/UsbService.cpp src/services/DisplayService.cpp src/services/SystemService.cpp src/services/NotesService.cpp
  src/services/NotificationService.cpp src/services/SecurityService.cpp src/services/DeviceLockService.cpp
  src/finance/BankClient.cpp src/finance/Money.cpp src/finance/FinanceDatabase.cpp src/finance/FinanceService.cpp
  src/finance/AnalyticsEngine.cpp src/finance/ImportExportManager.cpp src/finance/FinanceSecurityManager.cpp
)
set(PINE_UI_SOURCES
  src/ui/Font.cpp src/ui/Shell.cpp src/ui/HomeScreen.cpp src/ui/StatusBar.cpp
  src/ui/Navigation.cpp src/ui/DeveloperPanel.cpp src/ui/PinScreen.cpp src/ui/WifiScreen.cpp
  src/input/OnScreenKeyboard.cpp
  src/apps/SettingsApp.cpp src/apps/FilesApp.cpp src/apps/CameraApp.cpp src/apps/BluetoothApp.cpp src/apps/NotesApp.cpp src/apps/FinanceApp.cpp
  src/apps/finance/FinanceBanks.cpp src/apps/finance/FinanceViews.cpp src/apps/finance/FinanceWorkflows.cpp
)
