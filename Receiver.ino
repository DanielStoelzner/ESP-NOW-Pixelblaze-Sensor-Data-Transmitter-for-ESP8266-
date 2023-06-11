#include <ESP8266WiFi.h>
#include <espnow.h>

const uint8_t LED_PIN = 2;

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

int t1 = millis();

void OnDataRecv(uint8_t * mac, uint8_t *incomingData, uint8_t len) {
	memcpy(&dataFrame, incomingData, sizeof(dataFrame));
	if (memcmp(pairRequestFrame.headerTag, incomingData, 5) == 0){
		esp_now_send(mac, (uint8_t *) &pairAnswerFrame, sizeof(pairAnswerFrame));
	} else {
		Serial.write((uint8_t *) &dataFrame,sizeof(dataFrame) - 1);
		if(dataFrame.flash == 1){
			digitalWrite(LED_PIN, 0);
			t1 = millis();
		} else {
			digitalWrite(LED_PIN, 1);
		}
	}
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
  
	esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
	esp_now_register_recv_cb(OnDataRecv);

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
	
	uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
	esp_now_send(broadcastAddress, (uint8_t *) &pairAnswerFrame, sizeof(pairAnswerFrame));
}

void loop() {
	if(millis() - t1 > 1000){
		digitalWrite(LED_PIN, 1);
	}
}