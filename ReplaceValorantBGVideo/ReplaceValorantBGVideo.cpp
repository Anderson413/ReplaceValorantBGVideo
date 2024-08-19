#include "ReplaceValorantBGVideo.h"
#include <QFileDialog>
#include <Windows.h>
#include <TlHelp32.h>
#include <qmessagebox.h>
#include <sddl.h>
#include <atlbase.h>

ReplaceValorantBGVideo::ReplaceValorantBGVideo(QWidget *parent)
	: QMainWindow(parent)
{
	ui.setupUi(this);
	// init DebugWindow size
	dw_widget.setFixedSize(dw_size);
	dw_lw->setFixedSize(dw_size);

	this->setWindowFlags(windowFlags()& ~Qt::WindowMaximizeButtonHint);
	this->setMaximumSize(970, 331);
	this->setMinimumSize(970, 331);
	ui.btn_import->setDisabled(1);
	
	timer->start(1000);
	connect(timer, &QTimer::timeout, this, &ReplaceValorantBGVideo::onTimeout); //检测瓦罗兰特是否运行中 如运行则继续进一步工作

	connect(ui.select_gamedir, &QToolButton::clicked, this, [this](){
		QString dir;
		if (!ui.input_gamedir->text().isEmpty()) {
			dir = ui.input_gamedir->text();
		}
		GameDir = QFileDialog::getOpenFileName(this, "选择本赛季的视频文件", dir, "Video File(*.mp4)");
		if (!GameDir.isEmpty()) ui.input_gamedir->setText(GameDir);
	});

	connect(ui.select_video, &QToolButton::clicked, this, [this]() {
		QString dir;
		if (!ui.input_videodir->text().isEmpty()) {
			dir = ui.input_videodir->text();
		}
		if (!checkBox_Dir_isEnable) VideoDir = QFileDialog::getOpenFileName(this, "选择视频文件", dir, "Video File(*.mp4)");
		else VideoDir = QFileDialog::getExistingDirectory(this, "选择视频所在的文件夹");
		if (!VideoDir.isEmpty())
		{
			ui.input_videodir->setText(VideoDir);
		}
	});

	connect(ui.input_gamedir, &QLineEdit::textChanged, this, [this]() {
		GameDir = ui.input_gamedir->text();
	});

	connect(ui.input_videodir, &QLineEdit::textChanged, this, [this]() {
		VideoDir = ui.input_videodir->text();
	});

	connect(ui.btn_Preview_0, &QPushButton::clicked, this, [this]() {
		playVideo(GameDir);
	});

	connect(ui.btn_Preview_1, &QPushButton::clicked, this, [this]() {
		QListWidgetItem* item = ui.videoList->currentItem();
		if (item) {
			playVideo(item->whatsThis());
		}
	});

	connect(ui.btn_stopPreview, &QPushButton::clicked, this, [this]() {
		player->stop();
	});

	connect(ui.btn_AutoDetect, &QPushButton::clicked, this, [this]() {
		QString path;
		getGamePath(path);
		ui.input_gamedir->setText(path);
		qDebug() << path;
	});

	connect(ui.VList_Add, &QPushButton::clicked, this, [this]() {
		QListWidgetItem* item = new QListWidgetItem(ui.videoList);
		item->setText(QFileInfo(ui.input_videodir->text()).fileName());
		item->setWhatsThis(ui.input_videodir->text());
		ui.videoList->addItem(item);
	});

	connect(ui.VList_Del, &QPushButton::clicked, this, [this]() {
		QListWidgetItem* currentItem = ui.videoList->currentItem();
		if (currentItem)
		{
			int currentRow = ui.videoList->row(currentItem);
			delete ui.videoList->takeItem(currentRow);
		}
	});

	connect(ui.videoList, &QListWidget::itemDoubleClicked, this, &ReplaceValorantBGVideo::onItemDoubleClicked);

	connect(ui.btn_import, &QPushButton::clicked, this, &ReplaceValorantBGVideo::importVideos);

	connect(ui.checkBox_Folder, &QCheckBox::stateChanged, this, [=](int state) {
		if (state ==Qt::Checked)
		{
			ui.btn_import->setEnabled(1);
			checkBox_Dir_isEnable = true;
		}
		else if (state == Qt::Unchecked && checkBox_Dir_isClicked == true)
		{
			ui.videoList->clear();
			ui.btn_import->setDisabled(1);
			checkBox_Dir_isEnable = false;
			// recover
			for (int i = 0;i < saveOrginList.length();i++)
			{
				QListWidgetItem* recoverItem = new QListWidgetItem(ui.videoList);
				recoverItem->setText(QFileInfo(saveOrginList[i]).fileName());
				recoverItem->setWhatsThis(saveOrginList[i]);
			}
			saveNewList = {};
		}
	});
	
	connect(timer, &QTimer::timeout, this, [this]() {
		dont_Update_StatusTips = false;
	});

	connect(ui.btn_showDW, &QPushButton::clicked, this, &ReplaceValorantBGVideo::on_btn_showDWClicked);

	checkProfile(); // 检查配置文件是否存在 存在就弹窗提示是否加载

	// 设置状态栏
	QLabel* label = new QLabel;
	label->setOpenExternalLinks(true);
	label->setText("<a href=\"https://v.douyin.com/iMM6WAuG\">我要玩瓦罗兰特！我要玩瓦罗兰特！（打滚（</a>");
	ui.statusBar->setSizeGripEnabled(false);
	ui.statusBar->addPermanentWidget(label);

	pVideoWidget->setGeometry(20, 120, 320, 180);
	player->setVideoOutput(pVideoWidget);
}

ReplaceValorantBGVideo::~ReplaceValorantBGVideo()
{
	recoverVideo_onProcessClosed();
	saveProFile();
	delete player;
	delete timer;
}


void ReplaceValorantBGVideo::playVideo(QString File)
{
	player->setSource(QUrl::fromLocalFile(File));
	player->setLoops(1);
	player->play();
}

bool ReplaceValorantBGVideo::detectProcess(const wchar_t* ProcessName)
{
	bool exists = false;
	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hSnapshot != INVALID_HANDLE_VALUE) {
		PROCESSENTRY32 pe32;
		pe32.dwSize = sizeof(PROCESSENTRY32);

		if (Process32First(hSnapshot, &pe32)) {
			do {
				if (wcscmp(pe32.szExeFile, ProcessName) == 0) {
					exists = true;
					break;
				}
			} while (Process32Next(hSnapshot, &pe32));
		}
		CloseHandle(hSnapshot);
	}
	return exists;
}

void ReplaceValorantBGVideo::replaceVideo(QString File)
{
	dw_widget.show();	// 显示DebugWindow

	dw_info(QString("将要替换的文件: %1").arg(File));
	// 先删除temp文件夹再创建
	QDir temp, rmtemp("./temp");
	rmtemp.removeRecursively();
	temp.mkdir("temp");

	QString Video_FileName = QFileInfo(File).fileName(); // 文件名
	QString tmp_gameVideo = "./temp/" + GAMEFILE_NAME; // 合并路径

	// 备份原游戏视频文件
	QString backupFilePath = "./temp/" + GAMEFILE_NAME + ".bak";
	if (!QFile::copy(GameDir, backupFilePath)) {
		dw_warn("备份原游戏视频失败");
		ui.statusBar->showMessage("Warning: 备份原游戏视频失败");
		return;
	}
	else dw_done("备份原背景视频");

	// 复制替换视频到临时目录
	if (!QFile::copy(File, tmp_gameVideo)) {
		dw_warn("复制视频到临时目录失败");
		ui.statusBar->showMessage("Warning: 复制视频到临时目录失败");
		return;
	}
	else dw_done(QString("复制 %1 到临时目录").arg(Video_FileName));

	// 删除原游戏视频文件
	if (!QFile::remove(GameDir)) {
		dw_warn("删除原游戏背景视频文件失败");
		ui.statusBar->showMessage("Warning: 删除原游戏视频文件失败");
		return;
	}
	else dw_done("删除原背景视频");

	// 将临时目录中的视频复制回游戏目录
	if (!copyFile(QString(".\\temp\\%1").arg(GAMEFILE_NAME), GAMEPATH))
	{
		dw_warn("将 %1 复制到 背景视频目录失败");
		ui.statusBar->showMessage("Warning: 复制视频失败");
		qDebug() << QString("./temp/%1").arg(GAMEFILE_NAME);
		return;
	}
	else dw_done(QString("将 %1 复制到 背景视频目录").arg(Video_FileName));

	// 清理临时文件
	//rmtemp.removeRecursively();

	// 最终状态输出
	ui.statusBar->showMessage("VALORANT 已启动 - 文件替换成功！");
	dw_done("替换成功:");
	dw_text(QString("\t备份文件在: %1").arg(backupFilePath));
	dw_text(QString("\t新背景视频已替换至: %1").arg(GAMEPATH));

	int l = static_cast<int>(whoislongestString() * 4);
	dw_widget.setFixedSize(dw_size.width() + l, dw_size.height());
	dw_lw->setFixedSize(dw_size.width() + l, dw_size.height());
}

//游戏进行中时，如果意外关闭程序 也能恢复原文件
//避免了func_onTimeout_isExecuted未被设置成false 也能恢复文件
//为了不让启动器检测到文件丢失
void ReplaceValorantBGVideo::recoverVideo_onProcessClosed()
{
	if (func_onTimeout_isExecuted == true) //防止程序只是打开了 但是没有使用替换功能的时候 就错误的恢复原文件
	{
		ui.statusBar->showMessage("恢复原文件......");
		qDebug() << "恢复原文件";
		QFile recover(QString("./temp/%1.%2").arg(GAMEFILE_NAME, "bak"));
		if (recover.open(QIODevice::ReadWrite))
		{
			if (QFile::remove(QString("./temp/%1").arg(GAMEFILE_NAME)) && QFile::remove(GameDir))	// 删除视频文件
			{
				recover.rename(QString("./temp/%1").arg(GAMEFILE_NAME)); // 重命名备份文件
				recover.close();

				if (!copyFileQ(QString(".\\temp\\%1").arg(GAMEFILE_NAME), QString(GameDir).replace("/", "\\"))) {
					ui.statusBar->showMessage("Warning: 恢复失败");
					qDebug() << "恢复失败";
					return;
				}
			}
			else
			{
				return;
			}
		}
		else {
			ui.statusBar->showMessage("Warning: 未找到备份文件 请运行 无畏契约启动器 启动修补程序");
			return;
		}
		Sleep(500);
		if (DeleteFileA(static_cast<LPCSTR>(QString("./temp/%1").arg(GAMEFILE_NAME).toLocal8Bit())))
		{
			qDebug() << "删除temp目录下文件成功:" << QString("./temp/%1").arg(GAMEFILE_NAME);
		}
	}
}

void ReplaceValorantBGVideo::recoverVideo_onGameProcessClosed()
{
	if (func_onTimeout_isExecuted == true) //防止程序只是打开了 但是没有使用替换功能 就错误的恢复原文件
	{
		ui.statusBar->showMessage("恢复原文件......");
		dw_info("恢复原文件...");
		QFile recover(QString("./temp/%1.%2").arg(GAMEFILE_NAME, "bak"));
		if (recover.open(QIODevice::ReadWrite))
		{
			if (QFile::remove(QString("./temp/%1").arg(GAMEFILE_NAME)))	// 删除视频文件
			{
				recover.rename(QString("./temp/%1").arg(GAMEFILE_NAME)); // 重命名备份文件
				recover.close();

				if (!copyFileQ(QString(".\\temp\\%1").arg(GAMEFILE_NAME), QString(GameDir).replace("/", "\\"))) {
					ui.statusBar->showMessage("Warning: 恢复失败");
					dw_warn("恢复失败...");
					return;
				}
				QFile::remove(QString("./temp/%1").arg(GAMEFILE_NAME));
			}
			else
			{
				return;
			}
		}
		else {
			ui.statusBar->showMessage("Warning: 未找到备份文件 请运行 无畏契约启动器 启动修补程序");
			return;
		}
	}
	func_onTimeout_isExecuted = false;
	//恢复状态，以便在游戏最后一次退出时能不让recoverVideo_onProcessClosed()函数执行
	//为了阻止进行两次相同的工作
}

bool ReplaceValorantBGVideo::checkProfile()
{
	QFile file("ReplaceValorantBGVideo.ini");
	if (file.exists())
	{
		QMessageBox Message(QMessageBox::Question, "配置文件", "检测到配置文件，是否加载？", QMessageBox::Yes | QMessageBox::No);
		if (Message.exec() == QMessageBox::Yes)
		{
			loadProFile();
		}
	}
	CheckProfile_isClosed = true;
	return file.exists();
}

bool ReplaceValorantBGVideo::loadProFile()
{
	QSettings settings("ReplaceValorantBGVideo.ini", QSettings::IniFormat);
	
	// 视频文件
	settings.beginGroup("General");
	ui.input_gamedir->setText(settings.value("Game").toString());
	ui.input_videodir->setText(settings.value("Video").toString());
	settings.endGroup();

	settings.beginGroup("Button_State");
	
	// 文件夹checkbox
	bool folderIsEnable = settings.value("Folder_State").toBool();
	ui.checkBox_Folder->setChecked(folderIsEnable);

	if (folderIsEnable) ui.btn_import->setEnabled(1);
	else ui.btn_import->setDisabled(1);

	settings.endGroup();

	// 视频列表读取
	settings.beginGroup("VideoList");
	QString listEntry;
	if (!QString(settings.value("0").toString()).isEmpty())
	{
		int count = 0;
		do {
			listEntry = settings.value(QString::number(count)).toString();
			if (listEntry.isEmpty()) break; // 如果遇到空值则终止循环

			qDebug() << listEntry;

			// 更简洁地分割字符串
			QStringList parts = listEntry.split(",", Qt::KeepEmptyParts);
			if (parts.count() >= 2) {
				QString First = parts[0];
				QString Second = parts[1];

				qDebug() << "First:" << First;
				qDebug() << "Second:" << Second;

				// 添加到列表中
				QListWidgetItem* newItem = new QListWidgetItem(ui.videoList); // 直接设置文本
				if (First.isEmpty()) newItem->setText(QFileInfo(Second).fileName());
				else newItem->setText(First);
				newItem->setWhatsThis(Second); // 设置文件路径

			}

			count++; // 递增计数器
		} while (!listEntry.isEmpty());
	}

	settings.endGroup();

	return settings.isWritable();
}

bool ReplaceValorantBGVideo::saveProFile()
{
	proFile_Video = ui.input_videodir->text();
	proFile_Game = ui.input_gamedir->text();
	proFile_Folder_state = ui.checkBox_Folder->isChecked();

	QSettings settings("ReplaceValorantBGVideo.ini", QSettings::IniFormat);

	settings.beginGroup("General");
	settings.setValue("Game", proFile_Game);
	settings.setValue("Video", proFile_Video);
	settings.endGroup();

	settings.beginGroup("Button_State");
	settings.setValue("Folder_State", proFile_Folder_state);
	settings.endGroup();

	// 保存视频列表
	settings.beginGroup("VideoList");

	if (ui.videoList->count() > 0) {
		for (int i = 0; i < ui.videoList->count(); ++i) {
			QListWidgetItem* currentItem = ui.videoList->item(i); // 直接获取当前项，避免重复分配和内存泄漏
			settings.setValue(QString::number(i), QString("%1,%2").arg(currentItem->text(), currentItem->whatsThis()));
		}
	}
	else {
		ui.statusBar->showMessage("Warning: 视频列表里没有任何一个项，将不保存视频列表");
	}

	settings.endGroup();
	
	return settings.isWritable();
}

void ReplaceValorantBGVideo::importVideos()
{
	checkBox_Dir_isClicked = true;
	currentDir = ui.input_videodir->text();
	QListWidgetItem* item;
	//save
	if (ui.videoList->count() >= 1)
	{
		for (int i = 0; i < ui.videoList->count(); i++)
		{
			item = ui.videoList->item(i);
			QString tmp = item->whatsThis();
			saveOrginList.append(tmp);
		}
	}
	//import
	ui.videoList->clear();
	QDir dir(currentDir);
	dir.setFilter(QDir::Files);
	dir.setNameFilters(QStringList("*.mp4"));
	dir.setSorting(QDir::Name);
	saveNewList = dir.entryList();
	for (int i = 0;i < saveNewList.length();i++)
	{
		saveNewList.replace(i, QString(currentDir + "/" + saveNewList[i]));
		QListWidgetItem* brandNewItem = new QListWidgetItem(ui.videoList);
		brandNewItem->setText(QFileInfo(saveNewList[i]).fileName());
		brandNewItem->setWhatsThis(saveNewList[i]);
	}
}

bool ReplaceValorantBGVideo::getGamePath(QString& GamePath)
{
	QString sid;
	getUserSidString(sid);
	QString strReg = QString("HKEY_USERS\\%1\\SOFTWARE\\Tencent\\valorant.live").arg(sid);
	QSettings reg(strReg, QSettings::NativeFormat);
	QString dir = reg.value("InstallLocation").toString() + "\\ShooterGame\\Content\\Movies\\Menu";
	GamePath = dir;
	qDebug() << "InstallLocation:" << dir;
	return !dir.isEmpty();
}

int ReplaceValorantBGVideo::copyFile(QString File1, QString File2)
{
	QFile::remove("copyFile.bat");
	QFile file("copyFile.bat");
	if (file.exists()) {
		qDebug() << "Failed to remove existing file.";
		return false;
	}

	if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
		qDebug() << "Failed to open file for writing.";
		return false;
	}

	QTextStream str(&file);
	QString command = QString("copy \"%1\" \"%2\" /Y > nul").arg(File1, File2);
	str << command << "\n" << "if errorlevel 1 echo 0" << "\n" << "if errorlevel 0 echo 1" << "\n";
	file.close();
	
	QProcess proc;
	proc.startDetached("copyFile.bat");
	if (proc.waitForFinished(-1))
	{
		QString output = proc.readAllStandardOutput();
		int tmp = output.toInt();
		return tmp;
	}
}

bool ReplaceValorantBGVideo::copyFileQ(QString File1, QString File2)
{
	QString srcPath = QString(File1).replace("/", "\\");
	QString dstPath = QString(File2).replace("/", "\\");
	return CopyFileW(srcPath.toStdWString().c_str(), dstPath.toStdWString().c_str(), NULL);

}

void ReplaceValorantBGVideo::onItemDoubleClicked(QListWidgetItem* item)
{
	dont_Update_StatusTips = true;
	qDebug() << "将会在下一次 VALORANT 启动时更换此视频: " << item->text();
	statusTimer->start(1000);

	ui.statusBar->showMessage(QString("将会在下一次 VALORANT 启动时更换此视频: %1").arg(item->text()));
	currentVideo = item->whatsThis();
}

void ReplaceValorantBGVideo::onTimeout()
{
	bool processState = detectProcess(processName);

	if (processState && CheckProfile_isClosed) {
		if (!Game_State && !oneShot) {
			player->stop(); // 停止视频预览 节省CPU占用
			Game_State = true;
			oneShot = true;
			ui.statusBar->showMessage("VALORANT 已启动");
			func_onTimeout_isExecuted = true;

			QListWidgetItem* getItem;
			if (!currentVideo.isEmpty()) {
				replaceVideo(currentVideo);
			}
			else {
				// 如果currentVideo也未设置，则尝试获取索引为0的项
				getItem = ui.videoList->item(0);
				if (getItem) {
					replaceVideo(getItem->whatsThis());
				}
				else {
					dont_Update_StatusTips = true;
					ui.statusBar->showMessage("huh?: 好像什么都没有呢＞﹏＜");
				}
			}
		}
	}
	else {
		if (Game_State == true) recoverVideo_onGameProcessClosed();
		Game_State = false;
		if(!dont_Update_StatusTips) ui.statusBar->showMessage("等待 VALORANT 启动...........");
		dont_Update_StatusTips = true;
		oneShot = false;
	}
}

bool ReplaceValorantBGVideo::getUserSidString(QString& sidStr)
{
	HANDLE hToken;
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
		qWarning("Failed to open process token.");
		return false;
	}

	DWORD sidSize = 0;
	GetTokenInformation(hToken, TokenUser, nullptr, 0, &sidSize);
	if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
		CloseHandle(hToken);
		qWarning("Error in initial GetTokenInformation call.");
		return false;
	}

	PTOKEN_USER pTokenUser = (PTOKEN_USER)malloc(sidSize);
	if (pTokenUser == nullptr) {
		CloseHandle(hToken);
		qWarning("Memory allocation failed.");
		return false;
	}

	if (!GetTokenInformation(hToken, TokenUser, pTokenUser, sidSize, &sidSize)) {
		free(pTokenUser);
		CloseHandle(hToken);
		qWarning("Failed to get token information.");
		return false;
	}

	LPSTR sidBuffer = nullptr;
	if (!ConvertSidToStringSidA(pTokenUser->User.Sid, &sidBuffer)) {
		free(pTokenUser);
		CloseHandle(hToken);
		qWarning("Failed to convert SID to string.");
		return false;
	}

	sidStr = QString::fromLocal8Bit(sidBuffer);
	qDebug() << "User SID:" << sidStr;
	LocalFree(sidBuffer); // 释放由ConvertSidToStringSid分配的内存
	free(pTokenUser);
	CloseHandle(hToken);

	return true;
}

// ================= DEBUG WINDOW Area ==================

void ReplaceValorantBGVideo::dw_text(QString string)
{
	QListWidgetItem* item = new QListWidgetItem(dw_lw);
	item->setText(string);
}

void ReplaceValorantBGVideo::dw_info(QString string)
{
	QPixmap icon_info(":/ReplaceValorantBGVideo/Info.png");
	QListWidgetItem* item = new QListWidgetItem(dw_lw);
	item->setText(string);
	item->setIcon(icon_info);
}

void ReplaceValorantBGVideo::dw_warn(QString string)
{
	QPixmap icon_warn(":/ReplaceValorantBGVideo/Warning.png");
	QListWidgetItem* item = new QListWidgetItem(dw_lw);
	item->setText(string);
	item->setIcon(icon_warn);
}

void ReplaceValorantBGVideo::dw_done(QString string)
{
	QPixmap icon_done(":/ReplaceValorantBGVideo/Success.png");
	QListWidgetItem* item = new QListWidgetItem(dw_lw);
	item->setText(string);
	item->setIcon(icon_done);
}

int ReplaceValorantBGVideo::whoislongestString()
{
	int strLength = 0;
	for (int i = 0;i < dw_lw->count();i++)
	{
		QListWidgetItem item = *dw_lw->item(i);
		if (item.text().length() > strLength)
		{
			strLength = item.text().length();
		}
	}
	return strLength;
}

void ReplaceValorantBGVideo::on_btn_showDWClicked()
{
	if (dw_widget.isHidden())
	{
		dw_widget.show();
	}
	else dw_widget.hide();
	
}

// ================= DEBUG WINDOW Area ==================