FORMS += qtftpguiwidget.ui
HEADERS += qtftpgui.h qtftp.h
SOURCES += main.cpp qtftpgui.cpp qtftp.cpp
RESOURCES += qtftpgui.qrc
QTFTPGUI = app
CONFIG += release warn_on thread qt c++11
TARGET = qtftpgui
QT += network widgets
RC_FILE = qtftpgui.rc

win32 {
	LIBS += -static
}
