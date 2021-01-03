#include <map>

#include <logger.h>
#include <APRS-IS.h>
#include <TimeLib.h>

#include <TaskManager.h>

#include "BoardFinder.h"
#include "display.h"
#include "power_management.h"
#include "project_configuration.h"

#include "TaskAprsIs.h"
#include "TaskEth.h"
#include "TaskFTP.h"
#include "TaskLora.h"
#include "TaskNTP.h"
#include "TaskOTA.h"
#include "TaskWifi.h"

String create_lat_aprs(double lat);
String create_long_aprs(double lng);

std::shared_ptr<Configuration> userConfig;
std::shared_ptr<BoardConfig> boardConfig;
TaskManager taskManager;
HardwareSerial Serial(0);

// cppcheck-suppress unusedFunction
void setup()
{
	Serial.begin(115200);
	Logger::instance().setSerial(&Serial);
	delay(500);

	ProjectConfigurationManagement confmg;
	userConfig = confmg.readConfiguration();

	std::list<std::shared_ptr<BoardConfig>> boardConfigs;
	boardConfigs.push_back(std::shared_ptr<BoardConfig>(new BoardConfig("TTGO_LORA32_V1",         eTTGO_LORA32_V1,          4, 15, 0x3C,  0,  5, 19, 27, 18, 14, 26)));
	boardConfigs.push_back(std::shared_ptr<BoardConfig>(new BoardConfig("TTGO_LORA32_V2",         eTTGO_LORA32_V2,         21, 22, 0x3C,  0,  5, 19, 27, 18, 14, 26, true)));
	boardConfigs.push_back(std::shared_ptr<BoardConfig>(new BoardConfig("TTGO_T_Beam_V0_7",       eTTGO_T_Beam_V0_7,       21, 22, 0x3C,  0,  5, 19, 27, 18, 14, 26, true)));
	boardConfigs.push_back(std::shared_ptr<BoardConfig>(new BoardConfig("TTGO_T_Beam_V1_0",       eTTGO_T_Beam_V1_0,       21, 22, 0x3C,  0,  5, 19, 27, 18, 14, 26, true, true)));
	boardConfigs.push_back(std::shared_ptr<BoardConfig>(new BoardConfig("ETH_BOARD",              eETH_BOARD,              33, 32, 0x3C,  0, 14,  2, 15, 12,  4, 36)));
	boardConfigs.push_back(std::shared_ptr<BoardConfig>(new BoardConfig("TRACKERD",               eTRACKERD,                5,  4, 0x3C,  0, 18, 19, 23, 16, 14, 26)));
	boardConfigs.push_back(std::shared_ptr<BoardConfig>(new BoardConfig("HELTEC_WIFI_LORA_32_V1", eHELTEC_WIFI_LORA_32_V1,  4, 15, 0x3C, 16,  5, 19, 27, 18, 14, 26)));
	boardConfigs.push_back(std::shared_ptr<BoardConfig>(new BoardConfig("HELTEC_WIFI_LORA_32_V2", eHELTEC_WIFI_LORA_32_V2,  4, 15, 0x3C, 16,  5, 19, 27, 18, 14, 26)));

	BoardFinder finder(boardConfigs);
	boardConfig = finder.getBoardConfig(userConfig->board);
	if(boardConfig == 0)
	{
		boardConfig = finder.searchBoardConfig();
		if(boardConfig == 0)
		{
			logPrintlnE("Board config not set and search failed!");
			while(true)
			{}
		}
		userConfig->board = boardConfig->Name;
		confmg.writeConfiguration(userConfig);
		logPrintlnI("will restart board now!");
		ESP.restart();
	}
	logPrintI("Board ");
	logPrintI(boardConfig->Name);
	logPrintlnI(" loaded.");

	if(boardConfig->Type == eTTGO_T_Beam_V1_0)
	{
		TwoWire wire(0);
		wire.begin(boardConfig->OledSda, boardConfig->OledScl);
		std::shared_ptr<PowerManagement> powerManagement = std::shared_ptr<PowerManagement>(new PowerManagement);
		if (!powerManagement->begin(wire))
		{
			logPrintlnI("AXP192 init done!");
		}
		else
		{
			logPrintlnE("AXP192 init failed!");
		}
		powerManagement->activateLoRa();
		powerManagement->activateOLED();
		powerManagement->deactivateGPS();
	}

	logPrintlnW("LoRa APRS iGate by OE5BPA (Peter Buchegger)");
	logPrintlnW("Version: 20.49.0-dev");
	setup_display(boardConfig);
	show_display("OE5BPA", "LoRa APRS iGate", "by Peter Buchegger", "20.49.0-dev", 3000);

	load_config(boardConfig);

	std::shared_ptr<LoraTask> lora_task = std::shared_ptr<LoraTask>(new LoraTask());
	lora_task->setup(userConfig, boardConfig);
	taskManager.addTask(lora_task);
	if(boardConfig->Type == eETH_BOARD)
	{
		taskManager.addTask(std::shared_ptr<Task>(new EthTask()));
	}
	taskManager.addTask(std::shared_ptr<Task>(new WifiTask()));
	taskManager.addTask(std::shared_ptr<Task>(new OTATask()));
	taskManager.addTask(std::shared_ptr<Task>(new NTPTask()));
	taskManager.addTask(std::shared_ptr<Task>(new FTPTask()));
	taskManager.addTask(std::shared_ptr<Task>(new AprsIsTask()));

	taskManager.setup(userConfig);

	if(userConfig->display.overwritePin != 0)
	{
		pinMode(userConfig->display.overwritePin, INPUT);
		pinMode(userConfig->display.overwritePin, INPUT_PULLUP);
	}

	delay(500);
	logPrintlnI("setup done...");
}

// cppcheck-suppress unusedFunction
void loop()
{
	taskManager.loop(userConfig);
}

String create_lat_aprs(double lat)
{
	char str[20];
	char n_s = 'N';
	if(lat < 0)
	{
		n_s = 'S';
	}
	lat = std::abs(lat);
	sprintf(str, "%02d%05.2f%c", (int)lat, (lat - (double)((int)lat)) * 60.0, n_s);
	String lat_str(str);
	return lat_str;
}

String create_long_aprs(double lng)
{
	char str[20];
	char e_w = 'E';
	if(lng < 0)
	{
		e_w = 'W';
	}
	lng = std::abs(lng);
	sprintf(str, "%03d%05.2f%c", (int)lng, (lng - (double)((int)lng)) * 60.0, e_w);
	String lng_str(str);
	return lng_str;
}
