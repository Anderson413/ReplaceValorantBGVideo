#pragma once

#define GAMEPATH      QFileInfo(GameDir).absolutePath()
#define GAMEFILE_NAME QFileInfo(GameDir).fileName()

#include <QtWidgets/QMainWindow>
#include "ui_ReplaceValorantBGVideo.h"
#include <QtMultimedia/QMediaPlayer>
#include <QVideoWidget>
#include <Qtimer>
#include <qprocess.h>
#include <qfile.h>
#include <qsettings.h>
#include <QListWidgetItem>
#include <QWindow>

class ReplaceValorantBGVideo : public QMainWindow
{
	Q_OBJECT

public:
	ReplaceValorantBGVideo(QWidget *parent = nullptr);
	~ReplaceValorantBGVideo();

	QMediaPlayer*	player       = new QMediaPlayer(this);
	QVideoWidget*	pVideoWidget = new QVideoWidget(this);
	QTimer*			timer		 = new QTimer(this);
	QTimer*			statusTimer  = new QTimer(this);

	//Value
	QString GameDir, VideoDir;
	QString currentVideo;
	bool    Game_State = false;
	bool    CheckProfile_isClosed = false;
	bool    oneShot    = false;
	bool    dont_Update_StatusTips = false;
	bool	func_onTimeout_isExecuted = false;
	const wchar_t* processName = L"VALORANT-Win64-Shipping.exe";

	//Functions
	void playVideo(QString File);
	bool detectProcess(const wchar_t* ProcessName);
	void replaceVideo(QString File);
	void recoverVideo_onProcessClosed();
	void recoverVideo_onGameProcessClosed();
	bool getUserSidString(QString& sidStr);
	bool getGamePath(QString& GamePath);
	int  copyFile(QString File1, QString File2);
	bool copyFileQ(QString File1, QString File2);
	void onItemDoubleClicked(QListWidgetItem* item);
	void onTimeout();

	//ProFile
	bool        checkProfile();
	bool        loadProFile();
	bool        saveProFile();
	QString		proFile_Video;
	QString		proFile_Game;
	bool		proFile_Folder_state;
	QStringList proFile_VideoList;

	//Import videos
	QStringList saveOrginList;
	QStringList saveNewList;
	QString currentDir;
	bool checkBox_Dir_isEnable = false;
	bool checkBox_Dir_isClicked = false;
	void importVideos();

	// Debug Window
	QWidget dw_widget;
	QListWidget *dw_lw = new QListWidget(&dw_widget);
	QSize dw_size = QSize(400, 300);
	QString dw_longest_str_length;
	QSize dw_newSize;

	void dw_text(QString string);
	void dw_info(QString string);
	void dw_warn(QString string);
	void dw_done(QString string);
	int  whoislongestString();
	void on_btn_showDWClicked();

private:
	Ui::ReplaceValorantBGVideoClass ui;
};
