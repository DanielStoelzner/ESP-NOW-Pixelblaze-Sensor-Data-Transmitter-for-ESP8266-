#include <ESP8266WiFi.h>
#include <WifiEspNow.h>

const uint8_t LED_PIN = 2;
const uint8_t blinkEvery = 40;

uint8_t broadcastAddresses[1][6] = {};

typedef struct {
	uint8_t  headerTag[6];  
	uint16_t frequency[32];
	uint16_t energyAverage;
	uint16_t maxFreqMagnitude;
	uint16_t maxFrequency;
	int16_t accelerometer[3];
	uint16_t light;
	uint16_t analogInputs[5];
	uint8_t endTag[4];          // note: original data ends here
	uint8_t flash;              // additional 1 byte for synchronized flashing of on-board LED
} __attribute__((packed)) SensorPacket;
SensorPacket dataFrame;

typedef struct {
	uint8_t  headerTag[5];  
} __attribute__((packed)) PairRequestPacket;
PairRequestPacket pairRequestFrame;

typedef struct {
	uint8_t  headerTag[5];  
} __attribute__((packed)) PairAnswerPacket;
PairAnswerPacket pairAnswerFrame;

int cnt = 0;
uint8_t sendSuccess = 0;
uint8_t flash = 0;
const int MAX_PEERS = 20;

struct WifiEspNowPeerInfoExt {
	uint8_t mac[WIFIESPNOW_ALEN];
	uint8_t channel;
	uint8_t aya;
};

WifiEspNowPeerInfoExt peersExt[MAX_PEERS];

void OnDataRecv(const uint8_t mac[WIFIESPNOW_ALEN], const uint8_t* buf, size_t count, void* arg) {	
	Serial.printf("Message from %02X:%02X:%02X:%02X:%02X:%02X\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	for (int i = 0; i < static_cast<int>(count); ++i) {
		Serial.print(static_cast<char>(buf[i]));
	}
	if (memcmp(pairAnswerFrame.headerTag, buf, 5) == 0){
		WifiEspNowPeerInfo peers[MAX_PEERS];
		int nPeers = std::min(WifiEspNow.listPeers(peers, MAX_PEERS), MAX_PEERS);
		for (int i = 0; i < nPeers; ++i) {
			Serial.printf("1: %02X:%02X:%02X:%02X:%02X:%02X\n", peersExt[i].mac[0], peersExt[i].mac[1], peersExt[i].mac[2], peersExt[i].mac[3], peersExt[i].mac[4], peersExt[i].mac[5]);
			Serial.printf("2: %02X:%02X:%02X:%02X:%02X:%02X\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
			if (memcmp(peersExt[i].mac, mac, 6) == 0){
				peersExt[i].aya = false;
			}
		}
		bool ok = WifiEspNow.addPeer(mac);
		if (!ok) {
			ESP.restart();
		}
	}
}

void readBytes(uint8_t *buf, uint16_t size) {
	int i = 0;
	while (i < size) {
		if (Serial.available()) {
			*buf++ = Serial.read();
			i++;
		}
		else {
			delay(0);
		}
	}  
}

uint8_t readOneByte() {
	while (!Serial.available()) {
		delay(0);
	}
	return Serial.read();
}

bool readMagicWord() {
	if (readOneByte() != dataFrame.headerTag[0]) return false;
	if (readOneByte() != dataFrame.headerTag[1]) return false;
	if (readOneByte() != dataFrame.headerTag[2]) return false;
	if (readOneByte() != dataFrame.headerTag[3]) return false;
	if (readOneByte() != dataFrame.headerTag[4]) return false;
	if (readOneByte() != dataFrame.headerTag[5]) return false;
	return true;
}

void setup() {
	Serial.begin(115200);
	Serial.setRxBufferSize(1024); 

	pinMode(LED_PIN, OUTPUT);
	digitalWrite(LED_PIN, 1); 

	WiFi.mode(WIFI_STA);
	WiFi.disconnect();

	if (!WifiEspNow.begin() == false) {
		ESP.restart();
	}
	
	WifiEspNow.onReceive(OnDataRecv, nullptr);
	
	dataFrame.headerTag[0] = 'S';
	dataFrame.headerTag[1] = 'B';
	dataFrame.headerTag[2] = '1';
	dataFrame.headerTag[3] = '.';
	dataFrame.headerTag[4] = '0';    
	dataFrame.headerTag[5] = 0;

	pairRequestFrame.headerTag[0] = 'P';
	pairRequestFrame.headerTag[1] = 'R';
	pairRequestFrame.headerTag[2] = 'E';
	pairRequestFrame.headerTag[3] = 'Q';
	pairRequestFrame.headerTag[4] = 0;

	pairAnswerFrame.headerTag[0] = 'P';
	pairAnswerFrame.headerTag[1] = 'A';
	pairAnswerFrame.headerTag[2] = 'N';
	pairAnswerFrame.headerTag[3] = 'S';
	pairAnswerFrame.headerTag[4] = 0;

	pair();
}

void pair(){
	uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
	WifiEspNow.send(broadcastAddress, (uint8_t *) &pairRequestFrame, sizeof(pairRequestFrame));
}

int t1 = 0;
int t2 = 0;

void loop() {
	if(millis() - t2 > 1000){
		digitalWrite(LED_PIN, 1);
	}
	if (readMagicWord()) {
		readBytes((uint8_t *) &dataFrame.frequency, sizeof(SensorPacket)-6-1);
		if(flash){
			dataFrame.flash = 1;
		} else {
			dataFrame.flash = 0;
		}
		WifiEspNow.send(0, (uint8_t *) &dataFrame, sizeof(dataFrame));
		delay(10);
		if(WifiEspNow.getSendStatus() == WifiEspNowSendStatus::OK){
			t2 = millis();
			cnt++;
			if(cnt > 10){
				flash = 0;
			}
			if(cnt > blinkEvery){
				flash = 1;
				cnt = 0;
			}	
		}
	}
	if(millis() - t1 > 2000){
		WifiEspNowPeerInfo peers[MAX_PEERS];
		int nPeers = std::min(WifiEspNow.listPeers(peers, MAX_PEERS), MAX_PEERS);
		for (int i = 0; i < nPeers; ++i) {
			memcpy(peersExt[i].mac, peers[i].mac, sizeof(peers[i].mac));
			if(peersExt[i].aya){
				WifiEspNow.removePeer(peers[i].mac);
				peersExt[i].aya = false;
				continue;
			}
			peersExt[i].aya = true;
		}
		pair();
		t1 = millis();
	}
	if(flash){
		digitalWrite(LED_PIN, 0);
	} else {
		digitalWrite(LED_PIN, 1);
	}
}